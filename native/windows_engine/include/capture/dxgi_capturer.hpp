#pragma once

#include "capture/capturer_interface.hpp"
#include "metrics/metrics_tracker.hpp"

#include <memory>
#include <string>
#include <vector>

namespace secondscreen::capture {

struct MonitorInfo {
    uint32_t globalIndex{0};
    std::wstring deviceName{};
    int32_t left{0};
    int32_t top{0};
    uint32_t width{0};
    uint32_t height{0};
    bool isPrimary{false};
};

class DxgiCapturer : public ICapturer {
public:
    explicit DxgiCapturer(metrics::MetricsTracker* metrics = nullptr);
    ~DxgiCapturer() override;

    // Non-copyable, movable
    DxgiCapturer(const DxgiCapturer&) = delete;
    DxgiCapturer& operator=(const DxgiCapturer&) = delete;
    DxgiCapturer(DxgiCapturer&&) noexcept;
    DxgiCapturer& operator=(DxgiCapturer&&) noexcept;

    bool Initialize(const config::EngineConfig& config) override;
    bool AcquireFrame(uint32_t timeoutMs, CapturedFrame& outFrame) override;
    void ReleaseFrame() override;
    void Shutdown() override;

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] uint32_t GetOutputWidth() const noexcept;
    [[nodiscard]] uint32_t GetOutputHeight() const noexcept;
    [[nodiscard]] int32_t GetDesktopLeft() const noexcept;
    [[nodiscard]] int32_t GetDesktopTop() const noexcept;
    [[nodiscard]] uint32_t GetDxgiFormat() const noexcept;
    [[nodiscard]] const std::wstring& GetDeviceName() const noexcept;

    /// Enumerate all currently attached desktop outputs across all DXGI adapters.
    [[nodiscard]] static std::vector<MonitorInfo> EnumerateOutputs();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace secondscreen::capture
