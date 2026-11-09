#pragma once

#include "config/engine_config.hpp"
#include "pairing/pairing_manager.hpp"

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace secondscreen::engine {

using LogCallback = std::function<void(const std::string& line)>;

struct EngineOptions {
    uint16_t port{52025};
    uint32_t targetFps{60};
    uint32_t bitrateKbps{4000};
    bool adaptiveEnabled{true};
    uint32_t minBitrateKbps{1000};
    uint32_t maxBitrateKbps{8000};
    std::string pcName;
    int selectedMonitor{-1}; // -1 = auto-detect virtual/extended display
};

enum class ClientStatus {
    WaitingForClient,
    ClientConnecting,
    ClientInitializing,
    SendingInitialKeyframe,
    ClientStreaming,
    ClientDisconnected
};

class EngineRunner {
public:
    EngineRunner();
    ~EngineRunner();

    EngineRunner(const EngineRunner&) = delete;
    EngineRunner& operator=(const EngineRunner&) = delete;

    bool Start(const EngineOptions& options,
               secondscreen::pairing::PairingManager* pairingMgr,
               LogCallback logCb);

    void Stop();

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] ClientStatus GetClientStatus() const noexcept;
    [[nodiscard]] std::string GetClientIp() const;
    [[nodiscard]] std::string GetLastError() const;

private:
    void WorkerLoop(EngineOptions options,
                    secondscreen::pairing::PairingManager* pairingMgr,
                    LogCallback logCb);

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<ClientStatus> clientStatus_{ClientStatus::WaitingForClient};
    std::string clientIp_{};
    std::thread workerThread_;
    mutable std::mutex mutex_;
    std::string lastError_;
};

} // namespace secondscreen::engine

