#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <span>

namespace secondscreen::adaptive {

/// Network congestion classification
enum class NetworkHealth {
    Healthy,
    Congested,
    SeverelyCongested
};

/// High-level adaptation actions
enum class AdaptationAction {
    None,
    BitrateDecreased,
    BitrateIncreased,
    FpsDecreased,
    FpsIncreased
};

/// Record of an adaptation event
struct AdaptationEvent {
    AdaptationAction action{AdaptationAction::None};
    uint32_t oldBitrateKbps{0U};
    uint32_t newBitrateKbps{0U};
    uint32_t oldFps{0U};
    uint32_t newFps{0U};
    std::string reason;
    uint64_t timestampMs{0U};
};

/// Receiver telemetry report parsed from SS02 PACKET_TYPE_RECEIVER_REPORT
struct ReceiverFeedback {
    uint32_t receivedFps{0U};
    uint32_t decodedFps{0U};
    uint32_t droppedFrames{0U};
    uint32_t bufferDelayMs{0U};
    uint64_t lastReceivedTimestampMs{0U};
};

/// Configuration options for the adaptive streaming controller
struct AdaptiveConfig {
    bool enabled{true};
    uint32_t minBitrateKbps{1000U};
    uint32_t maxBitrateKbps{8000U};
    uint32_t initialBitrateKbps{4000U};

    std::vector<uint32_t> supportedFpsLevels{120U, 90U, 60U, 45U, 30U, 20U, 15U};
    uint32_t initialFps{30U};
    uint32_t minFps{15U};

    uint64_t sampleIntervalMs{1000U};      // Evaluation interval (1 sec)
    uint64_t cooldownPeriodMs{2000U};      // Min duration between adjustments (2 sec)
    uint64_t feedbackTimeoutMs{3000U};     // Threshold for marking feedback stale (3 sec)

    uint32_t congestionThresholdCount{2U}; // Consecutive congested intervals to trigger decrease
    uint32_t recoveryThresholdCount{4U};   // Consecutive healthy intervals to trigger increase

    double bitrateDecreaseFactor{0.75};    // Multiplicative decrease (-25%)
    uint32_t bitrateIncreaseStepKbps{500U};// Additive increase (+500 Kbps)

    // Thresholds
    uint32_t highBufferDelayMs{150U};      // Buffer delay above this indicates congestion
    uint32_t severeBufferDelayMs{250U};    // Buffer delay above this indicates severe congestion
    uint64_t highSendDurationUs{25000U};   // Send duration > 25ms indicates network queueing
    uint64_t severeSendDurationUs{45000U}; // Send duration > 45ms indicates severe queueing
};

/// Deterministic, low-latency adaptive bitrate and frame-rate controller
class AdaptiveStreamingController {
public:
    explicit AdaptiveStreamingController(const AdaptiveConfig& config = AdaptiveConfig{});
    ~AdaptiveStreamingController() = default;

    /// Configures or re-initializes controller
    void Initialize(const AdaptiveConfig& config);

    /// Records client telemetry from PACKET_TYPE_RECEIVER_REPORT (0x0008)
    bool OnReceiverReport(std::span<const uint8_t> payload, uint64_t currentTimestampMs);

    /// Directly injects receiver feedback struct (for tests and mocks)
    void SetReceiverFeedback(const ReceiverFeedback& feedback);

    /// Records host socket send duration for a frame
    void RecordSendDuration(uint64_t durationUs);

    /// Records socket buffer backpressure / write stalls
    void RecordSocketBackpressure(bool backpressure);

    /// Evaluates network condition and returns adaptation event if an adjustment occurs
    std::optional<AdaptationEvent> Evaluate(uint64_t currentTimestampMs);

    /// State queries
    [[nodiscard]] uint32_t GetCurrentBitrateKbps() const noexcept { return currentBitrateKbps_; }
    [[nodiscard]] uint32_t GetCurrentTargetFps() const noexcept { return currentFps_; }
    [[nodiscard]] NetworkHealth GetNetworkHealth() const noexcept { return currentHealth_; }
    [[nodiscard]] bool IsEnabled() const noexcept { return config_.enabled; }
    [[nodiscard]] const AdaptiveConfig& GetConfig() const noexcept { return config_; }
    [[nodiscard]] const std::vector<AdaptationEvent>& GetHistory() const noexcept { return eventHistory_; }

    void SetEnabled(bool enabled) noexcept { config_.enabled = enabled; }
    void Reset();

private:
    [[nodiscard]] NetworkHealth AssessNetworkHealth(uint64_t currentTimestampMs) const;
    void StepDownBitrate(const std::string& reason, uint64_t currentTimestampMs, std::optional<AdaptationEvent>& outEvent);
    void StepDownFps(const std::string& reason, uint64_t currentTimestampMs, std::optional<AdaptationEvent>& outEvent);
    void StepUpFps(const std::string& reason, uint64_t currentTimestampMs, std::optional<AdaptationEvent>& outEvent);
    void StepUpBitrate(const std::string& reason, uint64_t currentTimestampMs, std::optional<AdaptationEvent>& outEvent);

    AdaptiveConfig config_;
    uint32_t currentBitrateKbps_{4000U};
    uint32_t nominalFps_{30U};
    uint32_t currentFps_{30U};
    NetworkHealth currentHealth_{NetworkHealth::Healthy};

    uint64_t lastEvaluationTimeMs_{0U};
    uint64_t lastAdjustmentTimeMs_{0U};

    uint32_t consecutiveCongestedCount_{0U};
    uint32_t consecutiveHealthyCount_{0U};

    ReceiverFeedback latestFeedback_{};
    bool hasFeedback_{false};

    // Host transmission sliding metrics
    uint64_t totalSendDurationUsInInterval_{0U};
    uint32_t sendCountInInterval_{0U};
    uint32_t socketBackpressureCount_{0U};

    std::vector<AdaptationEvent> eventHistory_;
};

} // namespace secondscreen::adaptive

