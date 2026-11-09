#include "engine/engine_runner.hpp"

#include "protocol/protocol_constants.hpp"
#include "protocol/protocol_header.hpp"
#include "capture/dxgi_capturer.hpp"
#include "encoder/mf_h264_encoder.hpp"
#include "transport/tcp_transport_server.hpp"
#include "metrics/metrics_tracker.hpp"
#include "adaptive/adaptive_controller.hpp"
#include "discovery/udp_discovery_responder.hpp"
#include "input/input_injector.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>

namespace secondscreen::engine {

EngineRunner::EngineRunner() = default;

EngineRunner::~EngineRunner() {
    Stop();
}

bool EngineRunner::Start(const EngineOptions& options,
                         secondscreen::pairing::PairingManager* pairingMgr,
                         LogCallback logCb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRunning_) {
        lastError_ = "Engine is already running.";
        return false;
    }

    stopRequested_ = false;
    lastError_.clear();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    workerThread_ = std::thread(&EngineRunner::WorkerLoop, this, options, pairingMgr, std::move(logCb));
    
    // Give the worker thread a brief moment to initialize or report an immediate error
    for (int i = 0; i < 20; ++i) {
        if (isRunning_) return true;
        if (!lastError_.empty()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

void EngineRunner::Stop() {
    stopRequested_ = true;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    isRunning_ = false;
}

bool EngineRunner::IsRunning() const noexcept {
    return isRunning_.load();
}

ClientStatus EngineRunner::GetClientStatus() const noexcept {
    return clientStatus_.load();
}

std::string EngineRunner::GetClientIp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clientIp_;
}

std::string EngineRunner::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void EngineRunner::WorkerLoop(EngineOptions options,
                              secondscreen::pairing::PairingManager* pairingMgr,
                              LogCallback logCb) {
    auto log = [&](const std::string& msg) {
        if (logCb) {
            logCb(msg);
        }
    };

    log("[INFO] Starting in-process streaming engine...");

    // Fallback pairing manager if caller didn't supply one
    std::unique_ptr<secondscreen::pairing::PairingManager> fallbackPairing;
    if (!pairingMgr) {
        fallbackPairing = std::make_unique<secondscreen::pairing::PairingManager>();
        fallbackPairing->Initialize(options.pcName, options.port);
        pairingMgr = fallbackPairing.get();
    }

    secondscreen::metrics::MetricsTracker metrics;
    secondscreen::capture::DxgiCapturer capturer(&metrics);
    secondscreen::encoder::MfH264Encoder encoder(&metrics);
    secondscreen::transport::TcpTransportServer server;
    secondscreen::discovery::UdpDiscoveryResponder discoveryResponder;
    secondscreen::input::InputInjector inputInjector;

    secondscreen::adaptive::AdaptiveConfig adaptiveConfig{};
    adaptiveConfig.enabled = options.adaptiveEnabled;
    adaptiveConfig.initialBitrateKbps = options.bitrateKbps;
    adaptiveConfig.minBitrateKbps = options.minBitrateKbps;
    adaptiveConfig.maxBitrateKbps = options.maxBitrateKbps;
    adaptiveConfig.initialFps = options.targetFps;
    adaptiveConfig.highSendDurationUs = 50000U;
    adaptiveConfig.severeSendDurationUs = 90000U;
    adaptiveConfig.congestionThresholdCount = 3U;
    secondscreen::adaptive::AdaptiveStreamingController adaptiveController(adaptiveConfig);

    secondscreen::config::EngineConfig config{};
    config.port = options.port;
    config.targetFps = options.targetFps;
    config.bitrateKbps = options.bitrateKbps;
    if (options.selectedMonitor >= 0) {
        config.monitorIndex = static_cast<uint32_t>(options.selectedMonitor);
        config.autoSelectVirtual = false;
    } else {
        config.autoSelectVirtual = true;
    }

    // 1. Initialize DXGI Capturer
    if (!capturer.Initialize(config)) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastError_ = "Failed to initialize DXGI Desktop Duplication capturer.";
        }
        log("[ERROR] " + lastError_);
        isRunning_ = false;
        return;
    }

    config.width = capturer.GetOutputWidth();
    config.height = capturer.GetOutputHeight();
    inputInjector.UpdateDisplayMetrics(config.width, config.height, capturer.GetDesktopLeft(), capturer.GetDesktopTop());
    
    std::string devName(capturer.GetDeviceName().begin(), capturer.GetDeviceName().end());
    log("[INFO] Source Display: " + devName + " (" + std::to_string(config.width) + "x" +
        std::to_string(config.height) + " at desktop offset " +
        std::to_string(capturer.GetDesktopLeft()) + "," + std::to_string(capturer.GetDesktopTop()) + ")");

    // 2. Outgoing packet callback
    uint32_t outSequence = 0;
    const auto onEncodedPacket = [&](const secondscreen::encoder::EncodedPacket& packet) {
        if (!server.IsClientConnected()) {
            return;
        }

        secondscreen::protocol::ProtocolHeader header{};
        header.magic = secondscreen::protocol::PROTOCOL_MAGIC_U32;
        header.version = secondscreen::protocol::PROTOCOL_VERSION_2;
        header.packetType = packet.type;
        header.sequenceNumber = ++outSequence;
        header.payloadLength = static_cast<uint32_t>(packet.data.size());
        header.timestampUs = packet.timestampUs;

        const auto sendStart = std::chrono::steady_clock::now();
        const bool sent = server.SendPacket(header, packet.data);
        const auto sendDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - sendStart).count();

        adaptiveController.RecordSendDuration(static_cast<uint64_t>(sendDurationUs));
        if (!sent) {
            adaptiveController.RecordSocketBackpressure(true);
        } else if (clientStatus_.load() == ClientStatus::SendingInitialKeyframe &&
                   packet.type == secondscreen::protocol::PacketType::Keyframe) {
            clientStatus_ = ClientStatus::ClientStreaming;
        }
    };

    // 3. Initialize H.264 Encoder
    if (!encoder.Initialize(config, onEncodedPacket)) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastError_ = "Failed to initialize Media Foundation H.264 Encoder.";
        }
        log("[ERROR] " + lastError_);
        capturer.Shutdown();
        isRunning_ = false;
        return;
    }

    log("[INFO] Active Encoder: " + encoder.GetEncoderName() +
        " (Hardware: " + (encoder.IsHardwareAccelerated() ? "YES" : "NO") + ")");

    // 4. Client message handler
    auto lastKeyframeReqTime = std::chrono::steady_clock::now();
    const auto onClientMessage = [&](const secondscreen::protocol::ProtocolHeader& header,
                                     std::span<const uint8_t> payload) {
        const auto now = std::chrono::steady_clock::now();
        const auto nowMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

        if (header.packetType == secondscreen::protocol::PacketType::KeyframeRequest) {
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastKeyframeReqTime).count();
            if (elapsedMs >= 300) {
                lastKeyframeReqTime = now;
                log("[INFO] Received upstream PACKET_TYPE_KEYFRAME_REQ. Resending Config and forcing IDR keyframe.");
                if (encoder.HasExtractedConfig()) {
                    const auto& spsPps = encoder.GetCodecConfig();
                    secondscreen::protocol::ProtocolHeader cfgHeader{};
                    cfgHeader.magic = secondscreen::protocol::PROTOCOL_MAGIC_U32;
                    cfgHeader.version = secondscreen::protocol::PROTOCOL_VERSION_2;
                    cfgHeader.packetType = secondscreen::protocol::PacketType::Config;
                    cfgHeader.sequenceNumber = ++outSequence;
                    cfgHeader.payloadLength = static_cast<uint32_t>(spsPps.size());
                    cfgHeader.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();

                    server.SendPacket(cfgHeader, spsPps);
                }
                encoder.RequestKeyframe();
            }
        } else if (header.packetType == secondscreen::protocol::PacketType::ReceiverReport) {
            adaptiveController.OnReceiverReport(payload, nowMs);
        } else if (header.packetType == secondscreen::protocol::PacketType::Heartbeat) {
            secondscreen::protocol::ProtocolHeader ackHeader{};
            ackHeader.magic = secondscreen::protocol::PROTOCOL_MAGIC_U32;
            ackHeader.version = secondscreen::protocol::PROTOCOL_VERSION_2;
            ackHeader.packetType = secondscreen::protocol::PacketType::HeartbeatAck;
            ackHeader.sequenceNumber = header.sequenceNumber;
            ackHeader.payloadLength = 0;
            ackHeader.timestampUs = header.timestampUs;
            server.SendPacket(ackHeader, {});
        } else if (header.packetType == secondscreen::protocol::PacketType::PairRequest) {
            std::string reqJson(reinterpret_cast<const char*>(payload.data()), payload.size());
            std::string token;
            size_t tokenPos = reqJson.find("\"token\":\"");
            if (tokenPos != std::string::npos) {
                size_t start = tokenPos + 9;
                size_t end = reqJson.find("\"", start);
                if (end != std::string::npos) {
                    token = reqJson.substr(start, end - start);
                }
            }

            const auto authStatus = pairingMgr->ValidatePairingToken(token);
            std::string respJson = "{\"status\":" + std::to_string(static_cast<uint32_t>(authStatus)) +
                                   ",\"name\":\"" + pairingMgr->GetPcName() +
                                   "\",\"id\":\"" + pairingMgr->GetPcId() + "\"}";

            secondscreen::protocol::ProtocolHeader respHeader{};
            respHeader.magic = secondscreen::protocol::PROTOCOL_MAGIC_U32;
            respHeader.version = secondscreen::protocol::PROTOCOL_VERSION_2;
            respHeader.packetType = secondscreen::protocol::PacketType::PairResponse;
            respHeader.sequenceNumber = ++outSequence;
            respHeader.payloadLength = static_cast<uint32_t>(respJson.size());
            respHeader.timestampUs = header.timestampUs;

            std::span<const uint8_t> respPayload(reinterpret_cast<const uint8_t*>(respJson.data()), respJson.size());
            server.SendPacket(respHeader, respPayload);

            if (authStatus == secondscreen::pairing::PairAuthStatus::Success) {
                log("[INFO] Paired successfully with Android client!");
            } else {
                log("[WARN] Client pairing rejected: token invalid or expired.");
            }
        } else if (header.packetType == secondscreen::protocol::PacketType::PointerEvent) {
            if (payload.size() >= 16) {
                const uint16_t evtType = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                const uint8_t button = payload[2];
                const uint8_t modifiers = payload[3];

                uint32_t rawX = (static_cast<uint32_t>(payload[4]) << 24) |
                                (static_cast<uint32_t>(payload[5]) << 16) |
                                (static_cast<uint32_t>(payload[6]) << 8) |
                                static_cast<uint32_t>(payload[7]);
                uint32_t rawY = (static_cast<uint32_t>(payload[8]) << 24) |
                                (static_cast<uint32_t>(payload[9]) << 16) |
                                (static_cast<uint32_t>(payload[10]) << 8) |
                                static_cast<uint32_t>(payload[11]);
                float normX = 0.0f;
                float normY = 0.0f;
                std::memcpy(&normX, &rawX, sizeof(float));
                std::memcpy(&normY, &rawY, sizeof(float));

                inputInjector.InjectPointerEvent(evtType, button, modifiers, normX, normY);
            }
        } else if (header.packetType == secondscreen::protocol::PacketType::ScrollEvent) {
            if (payload.size() >= 16) {
                uint32_t rawX = (static_cast<uint32_t>(payload[0]) << 24) |
                                (static_cast<uint32_t>(payload[1]) << 16) |
                                (static_cast<uint32_t>(payload[2]) << 8) |
                                static_cast<uint32_t>(payload[3]);
                uint32_t rawY = (static_cast<uint32_t>(payload[4]) << 24) |
                                (static_cast<uint32_t>(payload[5]) << 16) |
                                (static_cast<uint32_t>(payload[6]) << 8) |
                                static_cast<uint32_t>(payload[7]);
                float normX = 0.0f;
                float normY = 0.0f;
                std::memcpy(&normX, &rawX, sizeof(float));
                std::memcpy(&normY, &rawY, sizeof(float));

                int32_t deltaX = static_cast<int32_t>(
                    (static_cast<uint32_t>(payload[8]) << 24) |
                    (static_cast<uint32_t>(payload[9]) << 16) |
                    (static_cast<uint32_t>(payload[10]) << 8) |
                    static_cast<uint32_t>(payload[11]));
                int32_t deltaY = static_cast<int32_t>(
                    (static_cast<uint32_t>(payload[12]) << 24) |
                    (static_cast<uint32_t>(payload[13]) << 16) |
                    (static_cast<uint32_t>(payload[14]) << 8) |
                    static_cast<uint32_t>(payload[15]));

                inputInjector.InjectScrollEvent(normX, normY, deltaX, deltaY);
            }
        } else if (header.packetType == secondscreen::protocol::PacketType::KeyboardEvent) {
            if (payload.size() >= 12) {
                const uint16_t evtType = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
                const uint8_t modifiers = payload[2];
                const uint32_t keyCode = (static_cast<uint32_t>(payload[4]) << 24) |
                                         (static_cast<uint32_t>(payload[5]) << 16) |
                                         (static_cast<uint32_t>(payload[6]) << 8) |
                                         static_cast<uint32_t>(payload[7]);
                const uint32_t unicodeCodepoint = (static_cast<uint32_t>(payload[8]) << 24) |
                                                  (static_cast<uint32_t>(payload[9]) << 16) |
                                                  (static_cast<uint32_t>(payload[10]) << 8) |
                                                  static_cast<uint32_t>(payload[11]);

                inputInjector.InjectKeyboardEvent(evtType, modifiers, keyCode, unicodeCodepoint);
            }
        }
    };

    // 5. Client connection callbacks
    server.SetClientConnectedCallback([&]() {
        const std::string cIp = server.GetClientIp();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clientIp_ = cIp;
        }
        clientStatus_ = ClientStatus::ClientInitializing;
        log("[INFO] Client attached from " + cIp + ". Dispatching configuration and forcing initial IDR keyframe...");
        adaptiveController.Reset();

        if (encoder.HasExtractedConfig()) {
            const auto& spsPps = encoder.GetCodecConfig();
            secondscreen::protocol::ProtocolHeader cfgHeader{};
            cfgHeader.magic = secondscreen::protocol::PROTOCOL_MAGIC_U32;
            cfgHeader.version = secondscreen::protocol::PROTOCOL_VERSION_2;
            cfgHeader.packetType = secondscreen::protocol::PacketType::Config;
            cfgHeader.sequenceNumber = ++outSequence;
            cfgHeader.payloadLength = static_cast<uint32_t>(spsPps.size());
            cfgHeader.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            server.SendPacket(cfgHeader, spsPps);
        }
        clientStatus_ = ClientStatus::SendingInitialKeyframe;
        encoder.RequestKeyframe();
    });

    server.SetClientDisconnectedCallback([&]() {
        log("[INFO] Client disconnected. Releasing all held synthetic inputs.");
        inputInjector.ReleaseAll();
        clientStatus_ = ClientStatus::WaitingForClient;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clientIp_.clear();
        }
    });

    // 6. Start TCP Server & UDP Discovery Responder
    if (!server.Start(options.port, onClientMessage)) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastError_ = "Failed to start TCP Transport Server on port " + std::to_string(options.port);
        }
        log("[ERROR] " + lastError_);
        encoder.Shutdown();
        capturer.Shutdown();
        isRunning_ = false;
        return;
    }

    discoveryResponder.Start(52026, options.port);

    log("[INFO] Streaming engine online on TCP port " + std::to_string(options.port) +
        " (UDP discovery active on port 52026).");

    isRunning_ = true;

    auto frameDuration = std::chrono::microseconds(1000000 / options.targetFps);

    while (!stopRequested_) {
        const auto loopStart = std::chrono::steady_clock::now();
        const auto nowMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(loopStart.time_since_epoch()).count());

        if (server.IsClientConnected()) {
            // Adaptive streaming decision evaluation
            const auto adjEvent = adaptiveController.Evaluate(nowMs);
            if (adjEvent.has_value()) {
                if (adjEvent->action == secondscreen::adaptive::AdaptationAction::BitrateDecreased ||
                    adjEvent->action == secondscreen::adaptive::AdaptationAction::BitrateIncreased) {
                    encoder.SetBitrate(adjEvent->newBitrateKbps);
                } else if (adjEvent->action == secondscreen::adaptive::AdaptationAction::FpsDecreased ||
                           adjEvent->action == secondscreen::adaptive::AdaptationAction::FpsIncreased) {
                    options.targetFps = adjEvent->newFps;
                    encoder.SetTargetFps(options.targetFps);
                    frameDuration = std::chrono::microseconds(1000000 / options.targetFps);
                }
            }

            secondscreen::capture::CapturedFrame frame{};
            if (capturer.AcquireFrame(16, frame)) {
                encoder.Encode(frame);
            }
        } else {
            // Idle wait when no client is attached
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - loopStart);
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }

    // Graceful teardown
    discoveryResponder.Stop();
    server.Stop();
    encoder.Shutdown();
    capturer.Shutdown();
    inputInjector.ReleaseAll();

    clientStatus_ = ClientStatus::WaitingForClient;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clientIp_.clear();
    }

    isRunning_ = false;
    log("[INFO] Streaming engine stopped cleanly.");
}

} // namespace secondscreen::engine

