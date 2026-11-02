#include "adaptive/adaptive_controller.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace secondscreen::adaptive {

AdaptiveStreamingController::AdaptiveStreamingController(const AdaptiveConfig& config)
    : config_(config),
      currentBitrateKbps_(config.initialBitrateKbps),
      nominalFps_(config.initialFps),
      currentFps_(config.initialFps) {
    std::sort(config_.supportedFpsLevels.begin(), config_.supportedFpsLevels.end(), std::greater<uint32_t>());
}

void AdaptiveStreamingController::Initialize(const AdaptiveConfig& config) {
    config_ = config;
    std::sort(config_.supportedFpsLevels.begin(), config_.supportedFpsLevels.end(), std::greater<uint32_t>());
    currentBitrateKbps_ = std::clamp(config.initialBitrateKbps, config.minBitrateKbps, config.maxBitrateKbps);
    nominalFps_ = config.initialFps;
    currentFps_ = config.initialFps;
    Reset();
}

void AdaptiveStreamingController::Reset() {
    lastEvaluationTimeMs_ = 0U;
    lastAdjustmentTimeMs_ = 0U;
    consecutiveCongestedCount_ = 0U;
    consecutiveHealthyCount_ = 0U;
    latestFeedback_ = ReceiverFeedback{};
    hasFeedback_ = false;
    totalSendDurationUsInInterval_ = 0U;
    sendCountInInterval_ = 0U;
    socketBackpressureCount_ = 0U;
    currentHealth_ = NetworkHealth::Healthy;
    eventHistory_.clear();
}

bool AdaptiveStreamingController::OnReceiverReport(std::span<const uint8_t> payload, uint64_t currentTimestampMs) {
    // 20-byte payload: 5 x uint32 big-endian
    if (payload.size() < 20U) {
        return false;
    }

    const auto readU32 = [](const uint8_t* ptr) -> uint32_t {
        return (static_cast<uint32_t>(ptr[0]) << 24) |
               (static_cast<uint32_t>(ptr[1]) << 16) |
               (static_cast<uint32_t>(ptr[2]) << 8) |
               static_cast<uint32_t>(ptr[3]);
    };

    ReceiverFeedback fb{};
    fb.receivedFps = readU32(payload.data() + 0);
    fb.decodedFps = readU32(payload.data() + 4);
    fb.droppedFrames = readU32(payload.data() + 8);
    fb.bufferDelayMs = readU32(payload.data() + 12);
    fb.lastReceivedTimestampMs = currentTimestampMs;

    // Bounds checks for telemetry
    if (fb.receivedFps > 240U) fb.receivedFps = 240U;
    if (fb.decodedFps > 240U) fb.decodedFps = 240U;
    if (fb.bufferDelayMs > 10000U) fb.bufferDelayMs = 10000U;

    latestFeedback_ = fb;
    hasFeedback_ = true;

    return true;
}

void AdaptiveStreamingController::SetReceiverFeedback(const ReceiverFeedback& feedback) {
    latestFeedback_ = feedback;
    hasFeedback_ = true;
}

void AdaptiveStreamingController::RecordSendDuration(uint64_t durationUs) {
    totalSendDurationUsInInterval_ += durationUs;
    ++sendCountInInterval_;
}

void AdaptiveStreamingController::RecordSocketBackpressure(bool backpressure) {
    if (backpressure) {
        ++socketBackpressureCount_;
    }
}

NetworkHealth AdaptiveStreamingController::AssessNetworkHealth(uint64_t currentTimestampMs) const {
    bool feedbackCongested = false;
    bool feedbackSevere = false;

    // 1. Evaluate receiver telemetry if fresh
    if (hasFeedback_) {
        const uint64_t feedbackAgeMs = (currentTimestampMs >= latestFeedback_.lastReceivedTimestampMs)
            ? (currentTimestampMs - latestFeedback_.lastReceivedTimestampMs)
            : 0U;

        if (feedbackAgeMs <= config_.feedbackTimeoutMs) {
            // Buffer queue delay checks
            if (latestFeedback_.bufferDelayMs >= config_.severeBufferDelayMs) {
                feedbackSevere = true;
            } else if (latestFeedback_.bufferDelayMs >= config_.highBufferDelayMs) {
                feedbackCongested = true;
            }

            // Received FPS shortfall check: only flag congestion if receiver is actually dropping frames,
            // distinguishing real network packet loss from low motion / static desktop capture.
            if (currentFps_ > 10U && latestFeedback_.receivedFps > 0U && latestFeedback_.droppedFrames > 0U) {
                if (latestFeedback_.receivedFps < (currentFps_ * 40U / 100U)) {
                    feedbackSevere = true;
                } else if (latestFeedback_.receivedFps < (currentFps_ * 70U / 100U)) {
                    feedbackCongested = true;
                }
            }
        }
    }

    // 2. Evaluate local host transport metrics
    bool localCongested = false;
    bool localSevere = false;

    const uint64_t avgSendUs = (sendCountInInterval_ > 0U)
        ? (totalSendDurationUsInInterval_ / sendCountInInterval_)
        : 0U;

    if (socketBackpressureCount_ > 0U || avgSendUs >= config_.severeSendDurationUs) {
        localSevere = true;
    } else if (avgSendUs >= config_.highSendDurationUs) {
        localCongested = true;
    }

    if (feedbackSevere || localSevere) {
        return NetworkHealth::SeverelyCongested;
    }
    if (feedbackCongested || localCongested) {
        return NetworkHealth::Congested;
    }

    return NetworkHealth::Healthy;
}

std::optional<AdaptationEvent> AdaptiveStreamingController::Evaluate(uint64_t currentTimestampMs) {
    if (!config_.enabled) {
        return std::nullopt;
    }

    if (lastEvaluationTimeMs_ == 0U) {
        lastEvaluationTimeMs_ = currentTimestampMs;
        return std::nullopt;
    }

    const uint64_t elapsedSinceEval = currentTimestampMs - lastEvaluationTimeMs_;
    if (elapsedSinceEval < config_.sampleIntervalMs) {
        return std::nullopt;
    }

    lastEvaluationTimeMs_ = currentTimestampMs;
    currentHealth_ = AssessNetworkHealth(currentTimestampMs);

    // Reset interval accumulation counters
    totalSendDurationUsInInterval_ = 0U;
    sendCountInInterval_ = 0U;
    socketBackpressureCount_ = 0U;

    const uint64_t timeSinceLastAdj = (lastAdjustmentTimeMs_ > 0U)
        ? (currentTimestampMs - lastAdjustmentTimeMs_)
        : config_.cooldownPeriodMs;

    std::optional<AdaptationEvent> event = std::nullopt;

    if (currentHealth_ == NetworkHealth::Congested || currentHealth_ == NetworkHealth::SeverelyCongested) {
        consecutiveHealthyCount_ = 0U;
        ++consecutiveCongestedCount_;

        if (consecutiveCongestedCount_ >= config_.congestionThresholdCount && timeSinceLastAdj >= config_.cooldownPeriodMs) {
            // Sustained congestion detected
            consecutiveCongestedCount_ = 0U;
            lastAdjustmentTimeMs_ = currentTimestampMs;

            const bool atMinBitrate = (currentBitrateKbps_ <= config_.minBitrateKbps);

            if (currentHealth_ == NetworkHealth::SeverelyCongested && atMinBitrate) {
                // When bitrate is already minimized and severe congestion persists, step down target FPS
                StepDownFps("Severe congestion with bitrate at minimum", currentTimestampMs, event);
            } else {
                // Otherwise apply multiplicative decrease on bitrate
                const std::string reason = (currentHealth_ == NetworkHealth::SeverelyCongested)
                    ? "Severe transport backpressure"
                    : "Sustained network queueing";
                StepDownBitrate(reason, currentTimestampMs, event);
            }
        }
    } else {
        // Healthy network
        consecutiveCongestedCount_ = 0U;
        ++consecutiveHealthyCount_;

        if (consecutiveHealthyCount_ >= config_.recoveryThresholdCount && timeSinceLastAdj >= config_.cooldownPeriodMs) {
            consecutiveHealthyCount_ = 0U;
            lastAdjustmentTimeMs_ = currentTimestampMs;

            if (currentFps_ < nominalFps_) {
                // First restore FPS toward nominal target
                StepUpFps("Sustained network recovery", currentTimestampMs, event);
            } else if (currentBitrateKbps_ < config_.maxBitrateKbps) {
                // Additive increase on bitrate
                StepUpBitrate("Sustained network recovery", currentTimestampMs, event);
            }
        }
    }

    if (event.has_value()) {
        eventHistory_.push_back(*event);
    }

    return event;
}

void AdaptiveStreamingController::StepDownBitrate(const std::string& reason,
                                                 uint64_t currentTimestampMs,
                                                 std::optional<AdaptationEvent>& outEvent) {
    const uint32_t oldBitrate = currentBitrateKbps_;
    auto newBitrate = static_cast<uint32_t>(static_cast<double>(oldBitrate) * config_.bitrateDecreaseFactor);
    if (newBitrate < config_.minBitrateKbps) {
        newBitrate = config_.minBitrateKbps;
    }

    if (newBitrate != oldBitrate) {
        currentBitrateKbps_ = newBitrate;
        AdaptationEvent ev{};
        ev.action = AdaptationAction::BitrateDecreased;
        ev.oldBitrateKbps = oldBitrate;
        ev.newBitrateKbps = newBitrate;
        ev.oldFps = currentFps_;
        ev.newFps = currentFps_;
        ev.reason = reason;
        ev.timestampMs = currentTimestampMs;
        outEvent = ev;

        std::cout << "[ADAPTIVE] Bitrate decreased: " << oldBitrate << " -> " << newBitrate
                  << " Kbps (Reason: " << reason << ")\n";
    }
}

void AdaptiveStreamingController::StepDownFps(const std::string& reason,
                                             uint64_t currentTimestampMs,
                                             std::optional<AdaptationEvent>& outEvent) {
    const uint32_t oldFps = currentFps_;
    uint32_t targetFps = oldFps;

    // Find next lower FPS level
    for (size_t i = 0; i < config_.supportedFpsLevels.size(); ++i) {
        if (config_.supportedFpsLevels[i] < oldFps) {
            targetFps = config_.supportedFpsLevels[i];
            break;
        }
    }

    if (targetFps < config_.minFps) {
        targetFps = config_.minFps;
    }

    if (targetFps != oldFps) {
        currentFps_ = targetFps;
        AdaptationEvent ev{};
        ev.action = AdaptationAction::FpsDecreased;
        ev.oldBitrateKbps = currentBitrateKbps_;
        ev.newBitrateKbps = currentBitrateKbps_;
        ev.oldFps = oldFps;
        ev.newFps = targetFps;
        ev.reason = reason;
        ev.timestampMs = currentTimestampMs;
        outEvent = ev;

        std::cout << "[ADAPTIVE] Target FPS decreased: " << oldFps << " -> " << targetFps
                  << " FPS (Reason: " << reason << ")\n";
    }
}

void AdaptiveStreamingController::StepUpFps(const std::string& reason,
                                           uint64_t currentTimestampMs,
                                           std::optional<AdaptationEvent>& outEvent) {
    const uint32_t oldFps = currentFps_;
    uint32_t targetFps = oldFps;

    // Find next higher FPS level up to nominalFps
    for (int i = static_cast<int>(config_.supportedFpsLevels.size()) - 1; i >= 0; --i) {
        const uint32_t level = config_.supportedFpsLevels[static_cast<size_t>(i)];
        if (level > oldFps && level <= nominalFps_) {
            targetFps = level;
            break;
        }
    }

    if (targetFps != oldFps) {
        currentFps_ = targetFps;
        AdaptationEvent ev{};
        ev.action = AdaptationAction::FpsIncreased;
        ev.oldBitrateKbps = currentBitrateKbps_;
        ev.newBitrateKbps = currentBitrateKbps_;
        ev.oldFps = oldFps;
        ev.newFps = targetFps;
        ev.reason = reason;
        ev.timestampMs = currentTimestampMs;
        outEvent = ev;

        std::cout << "[ADAPTIVE] Target FPS increased: " << oldFps << " -> " << targetFps
                  << " FPS (Reason: " << reason << ")\n";
    }
}

void AdaptiveStreamingController::StepUpBitrate(const std::string& reason,
                                               uint64_t currentTimestampMs,
                                               std::optional<AdaptationEvent>& outEvent) {
    const uint32_t oldBitrate = currentBitrateKbps_;
    uint32_t newBitrate = oldBitrate + config_.bitrateIncreaseStepKbps;
    if (newBitrate > config_.maxBitrateKbps) {
        newBitrate = config_.maxBitrateKbps;
    }

    if (newBitrate != oldBitrate) {
        currentBitrateKbps_ = newBitrate;
        AdaptationEvent ev{};
        ev.action = AdaptationAction::BitrateIncreased;
        ev.oldBitrateKbps = oldBitrate;
        ev.newBitrateKbps = newBitrate;
        ev.oldFps = currentFps_;
        ev.newFps = currentFps_;
        ev.reason = reason;
        ev.timestampMs = currentTimestampMs;
        outEvent = ev;

        std::cout << "[ADAPTIVE] Bitrate increased: " << oldBitrate << " -> " << newBitrate
                  << " Kbps (Reason: " << reason << ")\n";
    }
}

} // namespace secondscreen::adaptive

