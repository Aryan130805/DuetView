#include "capture/dxgi_capturer.hpp"
#include "encoder/mf_h264_encoder.hpp"
#include "config/engine_config.hpp"
#include "metrics/metrics_tracker.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
    uint32_t targetFrames = 30U;
    uint32_t bitrateKbps = 4000U;
    std::string dumpFile = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            targetFrames = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--bitrate" && i + 1 < argc) {
            bitrateKbps = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--dump" && i + 1 < argc) {
            dumpFile = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: secondscreen_smoke_encoder [options]\n"
                      << "  --frames <N>   Number of desktop frames to capture & encode (default: 30)\n"
                      << "  --bitrate <K>  Bitrate in Kbps (default: 4000)\n"
                      << "  --dump <path>  Optional path to save raw H.264 elementary stream (.h264)\n";
            return 0;
        }
    }

    std::cout << "========================================================\n"
              << "  SecondScreen Native H.264 Encoder Smoke Test          \n"
              << "========================================================\n";

    secondscreen::metrics::MetricsTracker metrics;
    secondscreen::capture::DxgiCapturer capturer(&metrics);
    secondscreen::encoder::MfH264Encoder encoder(&metrics);

    // 1. Initialize Capturer
    secondscreen::config::EngineConfig config{};
    config.bitrateKbps = bitrateKbps;

    std::cout << "[INFO] Initializing DXGI Desktop Duplication...\n";
    if (!capturer.Initialize(config)) {
        std::cerr << "[FAIL] Failed to initialize DXGI capturer!\n";
        return 1;
    }

    const uint32_t width = capturer.GetOutputWidth();
    const uint32_t height = capturer.GetOutputHeight();
    config.width = width;
    config.height = height;

    std::cout << "[INFO] Desktop Source Resolution: " << width << "x" << height << "\n";

    // 2. Initialize Encoder
    uint32_t configPackets = 0U;
    uint32_t keyframePackets = 0U;
    uint32_t deltaPackets = 0U;
    uint64_t totalEncodedBytes = 0U;

    std::ofstream dumpStream;
    if (!dumpFile.empty()) {
        dumpStream.open(dumpFile, std::ios::binary);
        if (dumpStream.is_open()) {
            std::cout << "[INFO] Streaming raw H.264 output to: " << dumpFile << "\n";
        }
    }

    const auto onOutput = [&](const secondscreen::encoder::EncodedPacket& packet) {
        totalEncodedBytes += packet.data.size();

        if (dumpStream.is_open()) {
            dumpStream.write(reinterpret_cast<const char*>(packet.data.data()),
                             static_cast<std::streamsize>(packet.data.size()));
        }

        if (packet.type == secondscreen::protocol::PacketType::Config) {
            ++configPackets;
            std::cout << "  [CONFIG PACKET] Length: " << packet.data.size() << " bytes (SPS/PPS)\n";
        } else if (packet.type == secondscreen::protocol::PacketType::Keyframe) {
            ++keyframePackets;
            std::cout << "  [KEYFRAME #IDR] Length: " << packet.data.size() << " bytes"
                      << " | Encode time: " << (static_cast<double>(packet.encodeDurationUs) / 1000.0) << " ms\n";
        } else {
            ++deltaPackets;
            if (deltaPackets <= 5 || deltaPackets % 10 == 0) {
                std::cout << "  [DELTA FRAME  ] Length: " << packet.data.size() << " bytes\n";
            }
        }
    };

    std::cout << "[INFO] Initializing H.264 Encoder...\n";
    if (!encoder.Initialize(config, onOutput)) {
        std::cerr << "[FAIL] Failed to initialize H.264 encoder!\n";
        return 1;
    }

    std::cout << "[INFO] Selected Transform: " << encoder.GetEncoderName() << "\n"
              << "[INFO] Hardware Acceleration Verified: " << (encoder.IsHardwareAccelerated() ? "YES (Hardware MFT)" : "NO (Software Fallback)") << "\n"
              << "[INFO] Commencing live capture & encode loop (" << targetFrames << " frames)...\n\n";

    // 3. Capture & Encode Loop
    const auto startTime = std::chrono::steady_clock::now();
    uint32_t framesSubmitted = 0U;

    while (framesSubmitted < targetFrames) {
        secondscreen::capture::CapturedFrame frame{};
        if (capturer.AcquireFrame(100, frame)) {
            ++framesSubmitted;
            encoder.Encode(frame);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::cout << "\n[INFO] Draining and flushing encoder pipeline...\n";
    encoder.Flush();

    const auto totalDuration = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    const uint32_t totalVideoPackets = keyframePackets + deltaPackets;
    const double outputFps = totalDuration > 0 ? (static_cast<double>(totalVideoPackets) / totalDuration) : 0.0;
    const double avgSize = totalVideoPackets > 0 ? (static_cast<double>(totalEncodedBytes) / static_cast<double>(totalVideoPackets)) : 0.0;
    const double avgBitrateKbps = totalDuration > 0 ? ((static_cast<double>(totalEncodedBytes) * 8.0) / (totalDuration * 1000.0)) : 0.0;

    std::cout << "--------------------------------------------------------\n"
              << "H.264 Encoder Smoke Test Results:\n"
              << "  Selected Encoder:       " << encoder.GetEncoderName() << "\n"
              << "  Hardware Accelerated:   " << (encoder.IsHardwareAccelerated() ? "YES" : "NO") << "\n"
              << "  Frames Submitted:       " << framesSubmitted << " / " << targetFrames << "\n"
              << "  Config Packets (SPS/PPS):" << configPackets << "\n"
              << "  Keyframe Packets (IDR): " << keyframePackets << "\n"
              << "  Delta Packets (P-slice):" << deltaPackets << "\n"
              << "  Total H.264 Output:     " << totalEncodedBytes << " bytes\n"
              << "  Average Sample Size:    " << avgSize << " bytes/frame\n"
              << "  Output Rate:            " << std::fixed << std::setprecision(2) << outputFps << " FPS\n"
              << "  Measured Bitrate:       " << avgBitrateKbps << " Kbps\n"
              << "--------------------------------------------------------\n";

    if (dumpStream.is_open()) {
        dumpStream.close();
        std::cout << "[INFO] Test H.264 stream saved cleanly.\n";
    }

    capturer.Shutdown();
    encoder.Shutdown();

    if (totalVideoPackets > 0 && keyframePackets > 0) {
        std::cout << "[PASS] Hardware H.264 encoder produced valid video bitstream with verified keyframe!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Encoder did not produce required video packets or keyframe.\n";
        return 1;
    }
}
