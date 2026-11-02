#pragma once

#include "engine/engine_runner.hpp"
#include "pairing/pairing_manager.hpp"

#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace secondscreen::app {

using LogCallback = std::function<void(const std::string& line)>;

enum class HostState {
    Stopped,
    Starting,
    Running,
    Error
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    bool Start(const std::wstring& unusedEnginePath,
               uint16_t port,
               const std::string& pcName,
               uint32_t fps,
               uint32_t bitrateKbps,
               bool showConsole,
               int monitorIndex,
               LogCallback logCb);

    bool Start(uint16_t port,
               const std::string& pcName,
               uint32_t fps,
               uint32_t bitrateKbps,
               int monitorIndex,
               secondscreen::pairing::PairingManager* pairingMgr,
               LogCallback logCb);

    void Stop();
    [[nodiscard]] bool IsRunning() const;
    [[nodiscard]] HostState GetState() const;
    [[nodiscard]] std::string GetLastError() const;
    [[nodiscard]] uint32_t GetProcessId() const;

private:
    std::unique_ptr<secondscreen::engine::EngineRunner> engineRunner_;
    std::atomic<HostState> state_{HostState::Stopped};
    std::string lastError_;
    mutable std::mutex mutex_;
};

} // namespace secondscreen::app
