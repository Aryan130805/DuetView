#include "config/engine_config.hpp"

namespace secondscreen::config {

std::optional<std::string> EngineConfig::Validate() const noexcept {
    if (width < 640U || width > 7680U) {
        return "Width must be between 640 and 7680 pixels";
    }
    if (height < 480U || height > 4320U) {
        return "Height must be between 480 and 4320 pixels";
    }
    if (width % 2U != 0U) {
        return "Width must be an even integer for H.264 encoder alignment";
    }
    if (height % 2U != 0U) {
        return "Height must be an even integer for H.264 encoder alignment";
    }
    if (targetFps < 1U || targetFps > 120U) {
        return "Target FPS must be between 1 and 120";
    }
    if (bitrateKbps < 500U || bitrateKbps > 50000U) {
        return "Bitrate must be between 500 and 50000 Kbps";
    }
    if (keyframeIntervalFrames < 1U || keyframeIntervalFrames > 300U) {
        return "Keyframe interval must be between 1 and 300 frames";
    }
    if (port < 1024U) {
        return "Port must be in non-privileged user range (1024-65535)";
    }

    return std::nullopt;
}

} // namespace secondscreen::config
