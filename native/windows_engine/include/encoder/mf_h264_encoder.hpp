#pragma once

#include "encoder/encoder_interface.hpp"
#include "metrics/metrics_tracker.hpp"

#include <memory>
#include <string>

namespace secondscreen::encoder {

class MfH264Encoder : public IEncoder {
public:
    explicit MfH264Encoder(metrics::MetricsTracker* metrics = nullptr);
    ~MfH264Encoder() override;

    // Non-copyable, movable
    MfH264Encoder(const MfH264Encoder&) = delete;
    MfH264Encoder& operator=(const MfH264Encoder&) = delete;
    MfH264Encoder(MfH264Encoder&&) noexcept;
    MfH264Encoder& operator=(MfH264Encoder&&) noexcept;

    bool Initialize(const config::EngineConfig& config,
                    const EncodedOutputCallback& callback) override;
    bool Encode(const capture::CapturedFrame& frame) override;
    void RequestKeyframe() override;
    bool SetBitrate(uint32_t bitrateKbps) override;
    bool SetTargetFps(uint32_t targetFps) override;
    void Flush() override;
    void Shutdown() override;

    [[nodiscard]] bool IsHardwareAccelerated() const noexcept override;
    [[nodiscard]] const std::string& GetEncoderName() const noexcept override;
    [[nodiscard]] bool HasExtractedConfig() const noexcept;
    [[nodiscard]] const std::vector<uint8_t>& GetCodecConfig() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace secondscreen::encoder
