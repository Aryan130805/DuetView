#pragma once

#include "config/engine_config.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace secondscreen::capture {

struct CapturedFrame {
    void* d3d11Texture{nullptr};
    uint32_t width{0U};
    uint32_t height{0U};
    uint64_t timestampUs{0U};
    uint64_t frameNumber{0U};
    uint32_t dxgiFormat{87U}; // DXGI_FORMAT_B8G8R8A8_UNORM = 87
    bool isGpuTexture{true};
    uint64_t acquisitionDurationUs{0U};
};

using FrameCallback = std::function<void(const CapturedFrame& frame)>;

class ICapturer {
public:
    virtual ~ICapturer() = default;

    /// Initializes desktop duplication resources for the target monitor.
    virtual bool Initialize(const config::EngineConfig& config) = 0;

    /// Captures a single desktop frame within the specified timeout in milliseconds.
    /// Returns true if a new frame was acquired, false on timeout or error.
    virtual bool AcquireFrame(uint32_t timeoutMs, CapturedFrame& outFrame) = 0;

    /// Releases previously acquired frame back to the DXGI pipeline.
    virtual void ReleaseFrame() = 0;

    /// Shuts down capturer and releases DXGI / Direct3D 11 resources.
    virtual void Shutdown() = 0;
};

} // namespace secondscreen::capture
