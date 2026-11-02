#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace secondscreen::config {

struct EngineConfig {
    uint32_t monitorIndex{0U};
    bool autoSelectVirtual{true};
    uint32_t width{1920U};
    uint32_t height{1080U};
    uint32_t targetFps{30U};
    uint32_t bitrateKbps{6000U};
    uint32_t keyframeIntervalFrames{60U};
    uint16_t port{52025U};
    bool lowLatencyMode{true};
    bool preferHardwareEncoder{true};

    /// Validates all configuration constraints.
    /// Returns nullopt if valid, or a descriptive error string if invalid.
    [[nodiscard]] std::optional<std::string> Validate() const noexcept;
};

} // namespace secondscreen::config
