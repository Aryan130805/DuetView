#include "capture/dxgi_capturer.hpp"
#include "config/engine_config.hpp"
#include "metrics/metrics_tracker.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    uint32_t targetFrames = 30U;
    uint32_t monitorIndex = 0U;
    uint32_t timeoutMs = 100U;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            targetFrames = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--monitor" && i + 1 < argc) {
            monitorIndex = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--timeout" && i + 1 < argc) {
            timeoutMs = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (arg == "--help") {
            std::cout << "Usage: secondscreen_smoke_capture [options]\n"
                      << "  --frames <N>   Number of frames to capture (default: 30)\n"
                      << "  --monitor <I>  Output monitor index (default: 0)\n"
                      << "  --timeout <ms> Frame acquisition timeout in ms (default: 100)\n";
            return 0;
        }
    }

    std::cout << "========================================================\n"
              << "  SecondScreen DXGI Desktop Capture Smoke Test          \n"
              << "========================================================\n"
              << "Configuration:\n"
              << "  Target Frames: " << targetFrames << "\n"
              << "  Monitor Index: " << monitorIndex << "\n"
              << "  Acquisition Timeout: " << timeoutMs << " ms\n"
              << "--------------------------------------------------------\n";

    secondscreen::config::EngineConfig config{};
    config.monitorIndex = monitorIndex;

    secondscreen::metrics::MetricsTracker metrics;
    secondscreen::capture::DxgiCapturer capturer(&metrics);

    std::cout << "[INFO] Initializing Direct3D 11 and DXGI Desktop Duplication...\n";
    if (!capturer.Initialize(config)) {
        std::cerr << "[FAIL] Failed to initialize DXGI Desktop Duplication!\n";
        return 1;
    }

    std::wcout << L"[INFO] Attached Device: " << capturer.GetDeviceName() << L"\n";
    std::cout << "[INFO] Desktop Resolution: " << capturer.GetOutputWidth() << "x" << capturer.GetOutputHeight()
              << " (DXGI Format: " << capturer.GetDxgiFormat() << ")\n"
              << "[INFO] Commencing frame acquisition loop...\n\n";

    uint32_t acquiredCount = 0U;
    uint32_t timeoutCount = 0U;
    uint64_t totalAcquisitionUs = 0U;

    const auto overallStart = std::chrono::steady_clock::now();

    while (acquiredCount < targetFrames) {
        secondscreen::capture::CapturedFrame frame{};
        const bool acquired = capturer.AcquireFrame(timeoutMs, frame);

        if (acquired) {
            ++acquiredCount;
            totalAcquisitionUs += frame.acquisitionDurationUs;

            if (acquiredCount <= 5 || acquiredCount % 10 == 0 || acquiredCount == targetFrames) {
                std::cout << "  [FRAME #" << std::setw(3) << frame.frameNumber << "] "
                          << frame.width << "x" << frame.height
                          << " | GPU Texture: " << frame.d3d11Texture
                          << " | Acq Time: " << (static_cast<double>(frame.acquisitionDurationUs) / 1000.0) << " ms\n";
            }
        } else {
            ++timeoutCount;
        }

        // Slight sleep to simulate a typical frame pacing (~30 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    const auto overallDuration = std::chrono::duration<double>(std::chrono::steady_clock::now() - overallStart).count();
    const double effectiveFps = static_cast<double>(acquiredCount) / overallDuration;
    const double avgAcqMs = acquiredCount > 0 ? (static_cast<double>(totalAcquisitionUs) / (static_cast<double>(acquiredCount) * 1000.0)) : 0.0;

    std::cout << "\n--------------------------------------------------------\n"
              << "Capture Smoke Test Results:\n"
              << "  Frames Acquired:      " << acquiredCount << " / " << targetFrames << "\n"
              << "  Timeouts Encountered: " << timeoutCount << "\n"
              << "  Total Duration:       " << std::fixed << std::setprecision(2) << overallDuration << " s\n"
              << "  Effective Rate:       " << effectiveFps << " FPS\n"
              << "  Avg Acquisition Time: " << avgAcqMs << " ms\n"
              << "--------------------------------------------------------\n";

    std::cout << "[INFO] Shutting down and releasing DXGI / D3D11 resources...\n";
    capturer.Shutdown();
    std::cout << "[PASS] Smoke test completed successfully with clean resource shutdown.\n";

    return 0;
}
