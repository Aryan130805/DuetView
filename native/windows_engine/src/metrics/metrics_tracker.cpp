#include "metrics/metrics_tracker.hpp"

namespace secondscreen::metrics {

MetricsTracker::MetricsTracker() noexcept
    : lastSnapshotTime_(std::chrono::steady_clock::now()) {}

void MetricsTracker::RecordFrameCaptured() noexcept {
    ++capturedCount_;
    ++totalCapturedLifetime_;
}

void MetricsTracker::RecordFrameAcquisitionDuration(uint64_t durationUs) noexcept {
    acquisitionDurationTotalUs_ += durationUs;
}

void MetricsTracker::RecordCaptureTimeout() noexcept {
    ++timeoutsCount_;
    ++totalTimeoutsLifetime_;
}

void MetricsTracker::RecordCaptureError() noexcept {
    ++captureErrorsCount_;
    ++totalCaptureErrorsLifetime_;
}

void MetricsTracker::RecordRecoveryAttempt(bool success) noexcept {
    if (success) {
        ++recoveriesCount_;
        ++totalRecoveriesLifetime_;
    }
}

void MetricsTracker::RecordFrameEncoded(size_t payloadBytes, uint64_t encodeDurationUs) noexcept {
    ++encodedCount_;
    ++totalEncodedLifetime_;
    bytesSent_ += payloadBytes;
    totalBytesLifetime_ += payloadBytes;
    totalEncodeTimeUs_ += encodeDurationUs;
}

void MetricsTracker::RecordFrameDropped() noexcept {
    ++droppedCount_;
    ++totalDroppedLifetime_;
}

void MetricsTracker::RecordBytesSent(size_t bytes) noexcept {
    bytesSent_ += bytes;
    totalBytesLifetime_ += bytes;
}

MetricsSnapshot MetricsTracker::ComputeSnapshot() noexcept {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(now - lastSnapshotTime_).count();
    lastSnapshotTime_ = now;

    MetricsSnapshot snapshot{};
    if (elapsed > 0.0001) {
        const uint64_t cap = capturedCount_.exchange(0U);
        const uint64_t acqDurUs = acquisitionDurationTotalUs_.exchange(0U);
        const uint64_t enc = encodedCount_.exchange(0U);
        const uint64_t drp = droppedCount_.exchange(0U);
        const uint64_t bytes = bytesSent_.exchange(0U);
        const uint64_t encUs = totalEncodeTimeUs_.exchange(0U);
        timeoutsCount_.exchange(0U);
        captureErrorsCount_.exchange(0U);
        recoveriesCount_.exchange(0U);

        snapshot.captureFps = static_cast<double>(cap) / elapsed;
        snapshot.avgAcquisitionDurationMs = cap > 0 ? (static_cast<double>(acqDurUs) / (static_cast<double>(cap) * 1000.0)) : 0.0;
        snapshot.encodeFps = static_cast<double>(enc) / elapsed;
        snapshot.bitrateKbps = (static_cast<double>(bytes) * 8.0) / (elapsed * 1000.0);
        snapshot.avgEncodeTimeMs = enc > 0 ? (static_cast<double>(encUs) / (static_cast<double>(enc) * 1000.0)) : 0.0;
        (void)drp;
    }

    snapshot.totalFramesCaptured = totalCapturedLifetime_;
    snapshot.totalFramesEncoded = totalEncodedLifetime_;
    snapshot.totalFramesDropped = totalDroppedLifetime_;
    snapshot.totalBytesSent = totalBytesLifetime_;
    snapshot.totalTimeouts = totalTimeoutsLifetime_;
    snapshot.totalCaptureErrors = totalCaptureErrorsLifetime_;
    snapshot.totalRecoveries = totalRecoveriesLifetime_;

    return snapshot;
}

void MetricsTracker::Reset() noexcept {
    capturedCount_ = 0U;
    acquisitionDurationTotalUs_ = 0U;
    timeoutsCount_ = 0U;
    captureErrorsCount_ = 0U;
    recoveriesCount_ = 0U;

    encodedCount_ = 0U;
    droppedCount_ = 0U;
    bytesSent_ = 0U;
    totalEncodeTimeUs_ = 0U;

    totalCapturedLifetime_ = 0U;
    totalEncodedLifetime_ = 0U;
    totalDroppedLifetime_ = 0U;
    totalBytesLifetime_ = 0U;
    totalTimeoutsLifetime_ = 0U;
    totalCaptureErrorsLifetime_ = 0U;
    totalRecoveriesLifetime_ = 0U;

    lastSnapshotTime_ = std::chrono::steady_clock::now();
}

} // namespace secondscreen::metrics
