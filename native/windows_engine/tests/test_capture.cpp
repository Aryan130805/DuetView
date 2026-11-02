#include "capture/capturer_interface.hpp"
#include "capture/dxgi_capturer.hpp"
#include "config/engine_config.hpp"
#include "metrics/metrics_tracker.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int g_passedTests = 0;
int g_failedTests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++g_failedTests; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUN ] " << #fn << "...\n"; \
        const int prevFailures = g_failedTests; \
        fn(); \
        if (g_failedTests == prevFailures) { \
            std::cout << "  [PASS] " << #fn << "\n"; \
            ++g_passedTests; \
        } \
    } while (0)

// Test double for testing frame lifecycle and state transitions without physical display
class MockCapturer : public secondscreen::capture::ICapturer {
public:
    explicit MockCapturer(secondscreen::metrics::MetricsTracker* m = nullptr) : metrics_(m) {}

    bool Initialize(const secondscreen::config::EngineConfig& cfg) override {
        config_ = cfg;
        initialized_ = true;
        frameNumber_ = 0U;
        return true;
    }

    bool AcquireFrame(uint32_t timeoutMs, secondscreen::capture::CapturedFrame& outFrame) override {
        (void)timeoutMs;
        if (!initialized_) return false;

        if (simulateTimeout_) {
            if (metrics_) metrics_->RecordCaptureTimeout();
            return false;
        }

        const uint64_t fakeDurationUs = 1500ULL; // 1.5ms
        outFrame.d3d11Texture = reinterpret_cast<void*>(0xDEADBEEFULL);
        outFrame.width = config_.width;
        outFrame.height = config_.height;
        outFrame.dxgiFormat = 87U; // DXGI_FORMAT_B8G8R8A8_UNORM
        outFrame.isGpuTexture = true;
        outFrame.frameNumber = ++frameNumber_;
        outFrame.timestampUs = 1000000ULL + frameNumber_ * 33333ULL;
        outFrame.acquisitionDurationUs = fakeDurationUs;

        hasAcquired_ = true;
        if (metrics_) {
            metrics_->RecordFrameCaptured();
            metrics_->RecordFrameAcquisitionDuration(fakeDurationUs);
        }
        return true;
    }

    void ReleaseFrame() override {
        hasAcquired_ = false;
    }

    void Shutdown() override {
        ReleaseFrame();
        initialized_ = false;
        frameNumber_ = 0U;
    }

    void SetSimulateTimeout(bool val) noexcept { simulateTimeout_ = val; }
    [[nodiscard]] bool HasAcquiredFrame() const noexcept { return hasAcquired_; }

private:
    secondscreen::metrics::MetricsTracker* metrics_{nullptr};
    secondscreen::config::EngineConfig config_{};
    bool initialized_{false};
    bool hasAcquired_{false};
    bool simulateTimeout_{false};
    uint64_t frameNumber_{0U};
};

} // namespace

void TestCapturedFrameDefaults() {
    using namespace secondscreen::capture;

    CapturedFrame frame{};
    TEST_ASSERT(frame.d3d11Texture == nullptr, "Texture must default to nullptr");
    TEST_ASSERT(frame.width == 0U, "Width must default to 0");
    TEST_ASSERT(frame.height == 0U, "Height must default to 0");
    TEST_ASSERT(frame.frameNumber == 0U, "FrameNumber must default to 0");
    TEST_ASSERT(frame.dxgiFormat == 87U, "Format must default to DXGI_FORMAT_B8G8R8A8_UNORM (87)");
    TEST_ASSERT(frame.isGpuTexture == true, "isGpuTexture must default to true");
}

void TestCapturerStateTransitions() {
    secondscreen::metrics::MetricsTracker metrics;
    MockCapturer capturer(&metrics);

    secondscreen::capture::CapturedFrame frame{};

    // 1. Cannot acquire before Initialize
    TEST_ASSERT(!capturer.AcquireFrame(100, frame), "Acquire before Initialize must fail");

    // 2. Initialize with valid config
    secondscreen::config::EngineConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    TEST_ASSERT(capturer.Initialize(cfg), "Initialize must succeed");

    // 3. Acquire frames and verify sequence progression
    TEST_ASSERT(capturer.AcquireFrame(100, frame), "First frame acquisition must succeed");
    TEST_ASSERT(frame.frameNumber == 1U, "First frame sequence must be 1");
    TEST_ASSERT(frame.width == 1920U, "Width must match config");
    TEST_ASSERT(frame.height == 1080U, "Height must match config");
    TEST_ASSERT(capturer.HasAcquiredFrame(), "Capturer should be in acquired state");

    capturer.ReleaseFrame();
    TEST_ASSERT(!capturer.HasAcquiredFrame(), "Capturer must not be in acquired state after ReleaseFrame");

    TEST_ASSERT(capturer.AcquireFrame(100, frame), "Second frame acquisition must succeed");
    TEST_ASSERT(frame.frameNumber == 2U, "Second frame sequence must be 2");

    // 4. Shutdown resets state
    capturer.Shutdown();
    TEST_ASSERT(!capturer.AcquireFrame(100, frame), "Acquire after Shutdown must fail");
}

void TestCaptureTimeoutAndMetrics() {
    secondscreen::metrics::MetricsTracker metrics;
    MockCapturer capturer(&metrics);

    secondscreen::config::EngineConfig cfg{};
    capturer.Initialize(cfg);

    secondscreen::capture::CapturedFrame frame{};

    // Acquire 3 successful frames
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT(capturer.AcquireFrame(50, frame), "Frame must acquire successfully");
        capturer.ReleaseFrame();
    }

    // Simulate 2 timeouts (desktop static)
    capturer.SetSimulateTimeout(true);
    TEST_ASSERT(!capturer.AcquireFrame(50, frame), "Simulated timeout must return false");
    TEST_ASSERT(!capturer.AcquireFrame(50, frame), "Simulated timeout must return false");

    // Check aggregated snapshot
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto snapshot = metrics.ComputeSnapshot();

    TEST_ASSERT(snapshot.totalFramesCaptured == 3U, "Total captured frames must be 3");
    TEST_ASSERT(snapshot.totalTimeouts == 2U, "Total timeouts must be 2");
    TEST_ASSERT(snapshot.avgAcquisitionDurationMs > 0.0, "Average acquisition duration must be positive");
}

void TestDxgiCapturerLifecycle() {
    using namespace secondscreen::capture;

    secondscreen::metrics::MetricsTracker metrics;
    DxgiCapturer capturer(&metrics);

    TEST_ASSERT(!capturer.IsInitialized(), "New capturer must start uninitialized");
    TEST_ASSERT(capturer.GetOutputWidth() == 0U, "Output width must be 0 when uninitialized");
    TEST_ASSERT(capturer.GetOutputHeight() == 0U, "Output height must be 0 when uninitialized");

    // Safe shutdown when uninitialized
    capturer.Shutdown();
    TEST_ASSERT(!capturer.IsInitialized(), "Must remain uninitialized after Shutdown");
}

int main() {
    std::cout << "========================================================\n"
              << "  Running SecondScreen Capture Component Test Suite     \n"
              << "========================================================\n";

    RUN_TEST(TestCapturedFrameDefaults);
    RUN_TEST(TestCapturerStateTransitions);
    RUN_TEST(TestCaptureTimeoutAndMetrics);
    RUN_TEST(TestDxgiCapturerLifecycle);

    std::cout << "========================================================\n"
              << "  Capture Tests Summary: " << g_passedTests << " passed, " << g_failedTests << " failed\n"
              << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}
