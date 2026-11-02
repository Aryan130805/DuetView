#include "app/process_manager.hpp"

#include <windows.h>

namespace secondscreen::app {

ProcessManager::ProcessManager()
    : engineRunner_(std::make_unique<secondscreen::engine::EngineRunner>()) {}

ProcessManager::~ProcessManager() {
    Stop();
}

bool ProcessManager::Start(uint16_t port,
                           const std::string& pcName,
                           uint32_t fps,
                           uint32_t bitrateKbps,
                           int monitorIndex,
                           secondscreen::pairing::PairingManager* pairingMgr,
                           LogCallback logCb) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (IsRunning()) {
        lastError_ = "Engine is already running.";
        return false;
    }

    state_ = HostState::Starting;
    lastError_.clear();

    secondscreen::engine::EngineOptions options{};
    options.port = port;
    options.targetFps = fps;
    options.bitrateKbps = bitrateKbps;
    options.minBitrateKbps = 1000;
    options.maxBitrateKbps = (bitrateKbps < 8000) ? 8000 : (bitrateKbps * 2);
    options.adaptiveEnabled = true;
    options.pcName = pcName;
    options.selectedMonitor = monitorIndex;

    bool ok = engineRunner_->Start(options, pairingMgr, logCb);
    if (!ok) {
        state_ = HostState::Error;
        lastError_ = engineRunner_->GetLastError();
        if (lastError_.empty()) lastError_ = "Failed to start in-process streaming engine.";
        return false;
    }

    state_ = HostState::Running;
    return true;
}

bool ProcessManager::Start(const std::wstring& /*unusedEnginePath*/,
                           uint16_t port,
                           const std::string& pcName,
                           uint32_t fps,
                           uint32_t bitrateKbps,
                           bool /*showConsole*/,
                           int monitorIndex,
                           LogCallback logCb) {
    return Start(port, pcName, fps, bitrateKbps, monitorIndex, nullptr, std::move(logCb));
}

void ProcessManager::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engineRunner_) {
        engineRunner_->Stop();
    }
    state_ = HostState::Stopped;
}

bool ProcessManager::IsRunning() const {
    return engineRunner_ && engineRunner_->IsRunning();
}

HostState ProcessManager::GetState() const {
    if (state_ == HostState::Running && !IsRunning()) {
        return HostState::Stopped;
    }
    return state_.load();
}

std::string ProcessManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

uint32_t ProcessManager::GetProcessId() const {
    return static_cast<uint32_t>(GetCurrentProcessId());
}

} // namespace secondscreen::app
