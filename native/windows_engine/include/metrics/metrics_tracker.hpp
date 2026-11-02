#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace secondscreen::metrics {

struct MetricsSnapshot {
    double captureFps{0.0};
    double encodeFps{0.0};
    double bitrateKbps{0.0};
    double avgEncodeTimeMs{0.0};
    double avgAcquisitionDurationMs{0.0};
    uint64_t totalFramesCaptured{0U};
    uint64_t totalFramesEncoded{0U};
    uint64_t totalFramesDropped{0U};
    uint64_t totalBytesSent{0U};
    uint64_t totalTimeouts{0U};
    uint64_t totalCaptureErrors{0U};
    uint64_t totalRecoveries{0U};
};

class MetricsTracker {
public:
    MetricsTracker() noexcept;

    void RecordFrameCaptured() noexcept;
    void RecordFrameAcquisitionDuration(uint64_t durationUs) noexcept;
    void RecordCaptureTimeout() noexcept;
    void RecordCaptureError() noexcept;
    void RecordRecoveryAttempt(bool success) noexcept;

    void RecordFrameEncoded(size_t payloadBytes, uint64_t encodeDurationUs) noexcept;
    void RecordFrameDropped() noexcept;
    void RecordBytesSent(size_t bytes) noexcept;

    /// Resets sliding window counters and returns aggregated snapshot.
    [[nodiscard]] MetricsSnapshot ComputeSnapshot() noexcept;

    void Reset() noexcept;

private:
    std::chrono::steady_clock::time_point lastSnapshotTime_;

    std::atomic<uint64_t> capturedCount_{0U};
    std::atomic<uint64_t> acquisitionDurationTotalUs_{0U};
    std::atomic<uint64_t> timeoutsCount_{0U};
    std::atomic<uint64_t> captureErrorsCount_{0U};
    std::atomic<uint64_t> recoveriesCount_{0U};

    std::atomic<uint64_t> encodedCount_{0U};
    std::atomic<uint64_t> droppedCount_{0U};
    std::atomic<uint64_t> bytesSent_{0U};
    std::atomic<uint64_t> totalEncodeTimeUs_{0U};

    uint64_t totalCapturedLifetime_{0U};
    uint64_t totalEncodedLifetime_{0U};
    uint64_t totalDroppedLifetime_{0U};
    uint64_t totalBytesLifetime_{0U};
    uint64_t totalTimeoutsLifetime_{0U};
    uint64_t totalCaptureErrorsLifetime_{0U};
    uint64_t totalRecoveriesLifetime_{0U};
};

} // namespace secondscreen::metrics
