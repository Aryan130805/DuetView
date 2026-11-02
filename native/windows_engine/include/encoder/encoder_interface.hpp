#pragma once

#include "capture/capturer_interface.hpp"
#include "config/engine_config.hpp"
#include "protocol/protocol_constants.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace secondscreen::encoder {

struct EncodedPacket {
    protocol::PacketType type{protocol::PacketType::Delta};
    uint64_t timestampUs{0U};
    std::vector<uint8_t> data;
    bool isKeyframe{false};
    uint64_t encodeDurationUs{0U};
    uint32_t sequenceNumber{0U};
};

using EncodedOutputCallback = std::function<void(const EncodedPacket& packet)>;

class IEncoder {
public:
    virtual ~IEncoder() = default;

    /// Initializes the hardware or software encoder MFT with given configuration.
    virtual bool Initialize(const config::EngineConfig& config,
                            const EncodedOutputCallback& callback) = 0;

    /// Encodes a single D3D11 or CPU texture frame.
    virtual bool Encode(const capture::CapturedFrame& frame) = 0;

    /// Asynchronously requests the next encoded frame to be an IDR keyframe.
    virtual void RequestKeyframe() = 0;

    /// Dynamically updates the encoder target bitrate in Kbps.
    /// Returns true if dynamic reconfiguration was applied, or false if unsupported.
    virtual bool SetBitrate(uint32_t bitrateKbps) = 0;

    /// Dynamically updates the encoder target framerate in FPS.
    virtual bool SetTargetFps(uint32_t targetFps) = 0;

    /// Flushes any pending frames in the encoder pipeline.
    virtual void Flush() = 0;

    /// Releases encoder transforms and D3D11 device links.
    virtual void Shutdown() = 0;

    /// Returns true if currently running via a verified hardware-accelerated MFT.
    [[nodiscard]] virtual bool IsHardwareAccelerated() const noexcept = 0;

    /// Returns human-readable name of the active MFT encoder.
    [[nodiscard]] virtual const std::string& GetEncoderName() const noexcept = 0;
};

} // namespace secondscreen::encoder
