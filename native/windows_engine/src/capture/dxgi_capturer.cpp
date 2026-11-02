#include "capture/dxgi_capturer.hpp"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace secondscreen::capture {

using Microsoft::WRL::ComPtr;

struct DxgiCapturer::Impl {
    metrics::MetricsTracker* metrics{nullptr};
    config::EngineConfig config{};

    ComPtr<ID3D11Device> device{};
    ComPtr<ID3D11DeviceContext> context{};
    ComPtr<IDXGIFactory1> factory{};
    ComPtr<IDXGIAdapter1> adapter{};
    ComPtr<IDXGIOutput1> output1{};
    ComPtr<IDXGIOutputDuplication> duplication{};
    ComPtr<ID3D11Texture2D> retainedTexture{};

    std::wstring deviceName{};
    uint32_t outputWidth{0U};
    uint32_t outputHeight{0U};
    int32_t desktopLeft{0};
    int32_t desktopTop{0};
    DXGI_FORMAT outputFormat{DXGI_FORMAT_B8G8R8A8_UNORM};

    uint64_t frameCount{0U};
    bool isInitialized{false};
    bool hasAcquiredFrame{false};
    bool hasValidRetained{false};
    std::chrono::steady_clock::time_point lastFrameEmitTime{};
    uint32_t consecutiveErrors{0U};
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 5U;

    explicit Impl(metrics::MetricsTracker* m) : metrics(m) {}

    ~Impl() {
        Shutdown();
    }

    void EnsureInteractiveDesktop() {
        // When running under services, background agents, or sandboxed execution
        // desktops (e.g. exebox-*), DXGI DuplicateOutput requires attachment to the
        // interactive "default" desktop station.
        HDESK hDesk = OpenDesktopA("default", 0, FALSE, GENERIC_ALL);
        if (hDesk) {
            SetThreadDesktop(hDesk);
            CloseDesktop(hDesk);
        }
    }

    bool Initialize(const config::EngineConfig& cfg) {
        Shutdown();
        config = cfg;
        EnsureInteractiveDesktop();

        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            std::cerr << "[ERROR] CreateDXGIFactory1 failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        // Enumerate outputs to evaluate autoSelectVirtual
        auto availableMonitors = DxgiCapturer::EnumerateOutputs();
        if (config.autoSelectVirtual && config.monitorIndex == 0U && availableMonitors.size() > 1U) {
            for (const auto& mon : availableMonitors) {
                if (!mon.isPrimary) {
                    config.monitorIndex = mon.globalIndex;
                    std::cout << "[INFO] Multiple desktop displays detected (" << availableMonitors.size()
                              << "). Auto-selecting extended/virtual monitor #" << mon.globalIndex
                              << " (" << std::string(mon.deviceName.begin(), mon.deviceName.end())
                              << " " << mon.width << "x" << mon.height
                              << " at " << mon.left << "," << mon.top << ").\n";
                    break;
                }
            }
        }

        // Enumerate adapters and find target output
        ComPtr<IDXGIAdapter1> curAdapter;
        ComPtr<IDXGIOutput> curOutput;
        uint32_t currentGlobalOutputIndex = 0U;
        bool foundTarget = false;

        for (UINT aIdx = 0; factory->EnumAdapters1(aIdx, &curAdapter) != DXGI_ERROR_NOT_FOUND; ++aIdx) {
            DXGI_ADAPTER_DESC1 aDesc;
            curAdapter->GetDesc1(&aDesc);

            for (UINT oIdx = 0; curAdapter->EnumOutputs(oIdx, &curOutput) != DXGI_ERROR_NOT_FOUND; ++oIdx) {
                DXGI_OUTPUT_DESC oDesc;
                curOutput->GetDesc(&oDesc);

                if (oDesc.AttachedToDesktop) {
                    if (currentGlobalOutputIndex == config.monitorIndex) {
                        adapter = curAdapter;
                        deviceName = oDesc.DeviceName;
                        outputWidth = static_cast<uint32_t>(oDesc.DesktopCoordinates.right - oDesc.DesktopCoordinates.left);
                        outputHeight = static_cast<uint32_t>(oDesc.DesktopCoordinates.bottom - oDesc.DesktopCoordinates.top);
                        desktopLeft = oDesc.DesktopCoordinates.left;
                        desktopTop = oDesc.DesktopCoordinates.top;

                        hr = curOutput.As(&output1);
                        if (SUCCEEDED(hr)) {
                            foundTarget = true;
                            break;
                        }
                    }
                    ++currentGlobalOutputIndex;
                }
            }
            if (foundTarget) break;
        }

        // Fallback to first attached output if configured monitorIndex is out of range
        if (!foundTarget && currentGlobalOutputIndex > 0U) {
            std::cerr << "[WARN] Configured monitor index " << config.monitorIndex
                      << " not found. Falling back to default primary output #0.\n";
            config.monitorIndex = 0U;
            return Initialize(config);
        }

        if (!foundTarget || !output1) {
            std::cerr << "[ERROR] No desktop output monitor found for duplication.\n";
            return false;
        }

        // Create Direct3D 11 hardware device
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL obtainedLevel{};

        hr = D3D11CreateDevice(
            adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN, // Using explicit DXGI adapter
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &device,
            &obtainedLevel,
            &context
        );

        if (FAILED(hr)) {
            std::cerr << "[ERROR] D3D11CreateDevice failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        // Duplicate desktop output
        hr = output1->DuplicateOutput(device.Get(), &duplication);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] DuplicateOutput failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        DXGI_OUTDUPL_DESC dupDesc;
        duplication->GetDesc(&dupDesc);
        outputWidth = dupDesc.ModeDesc.Width;
        outputHeight = dupDesc.ModeDesc.Height;
        outputFormat = dupDesc.ModeDesc.Format;

        // Create GPU-resident retained texture
        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = outputWidth;
        texDesc.Height = outputHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = outputFormat;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

        hr = device->CreateTexture2D(&texDesc, nullptr, &retainedTexture);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] CreateTexture2D for retained GPU texture failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        isInitialized = true;
        hasValidRetained = false;
        consecutiveErrors = 0U;
        lastFrameEmitTime = std::chrono::steady_clock::now();

        // Attempt initial frame acquisition to pre-seed retainedTexture
        DXGI_OUTDUPL_FRAME_INFO initInfo{};
        ComPtr<IDXGIResource> initRes;
        if (duplication->AcquireNextFrame(250, &initInfo, &initRes) == S_OK) {
            ComPtr<ID3D11Texture2D> initTex;
            if (initRes && SUCCEEDED(initRes.As(&initTex)) && initTex) {
                context->CopyResource(retainedTexture.Get(), initTex.Get());
                hasValidRetained = true;
            }
            duplication->ReleaseFrame();
        }

        return true;
    }

    bool AcquireFrame(uint32_t timeoutMs, CapturedFrame& outFrame) {
        if (!isInitialized) {
            return false;
        }

        // Ensure any previous unreleased frame is cleanly released
        ReleaseFrame();

        const auto start = std::chrono::steady_clock::now();

        DXGI_OUTDUPL_FRAME_INFO frameInfo{};
        ComPtr<IDXGIResource> desktopResource;

        HRESULT hr = duplication->AcquireNextFrame(timeoutMs, &frameInfo, &desktopResource);

        const auto elapsedUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count()
        );

        if (hr == S_OK) {
            hasAcquiredFrame = true;
            consecutiveErrors = 0U;

            ComPtr<ID3D11Texture2D> acquiredTexture;
            if (desktopResource) {
                desktopResource.As(&acquiredTexture);
            }

            if (acquiredTexture) {
                // Direct GPU blit to retained texture (100% inside VRAM, zero CPU readback)
                context->CopyResource(retainedTexture.Get(), acquiredTexture.Get());
                hasValidRetained = true;
            }

            // Release DXGI frame lock immediately so desktop duplication pipeline is never blocked
            duplication->ReleaseFrame();
            hasAcquiredFrame = false;

            if (hasValidRetained) {
                const auto nowUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()
                );

                outFrame.d3d11Texture = retainedTexture.Get();
                outFrame.width = outputWidth;
                outFrame.height = outputHeight;
                outFrame.dxgiFormat = static_cast<uint32_t>(outputFormat);
                outFrame.isGpuTexture = true;
                outFrame.frameNumber = ++frameCount;
                outFrame.timestampUs = nowUs;
                outFrame.acquisitionDurationUs = elapsedUs;

                lastFrameEmitTime = std::chrono::steady_clock::now();

                if (metrics) {
                    metrics->RecordFrameCaptured();
                    metrics->RecordFrameAcquisitionDuration(elapsedUs);
                }

                return true;
            }

            return false;
        }

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            if (metrics) {
                metrics->RecordCaptureTimeout();
            }

            // Keepalive / static desktop refresh:
            // Emit retained texture if stream is idle >= 500ms or on the very first frame
            const auto now = std::chrono::steady_clock::now();
            const auto idleElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastFrameEmitTime).count();

            if (hasValidRetained && (frameCount == 0 || idleElapsedMs >= 500)) {
                const auto nowUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        now.time_since_epoch()
                    ).count()
                );

                outFrame.d3d11Texture = retainedTexture.Get();
                outFrame.width = outputWidth;
                outFrame.height = outputHeight;
                outFrame.dxgiFormat = static_cast<uint32_t>(outputFormat);
                outFrame.isGpuTexture = true;
                outFrame.frameNumber = ++frameCount;
                outFrame.timestampUs = nowUs;
                outFrame.acquisitionDurationUs = elapsedUs;

                lastFrameEmitTime = now;
                return true;
            }

            return false;
        }

        // Recoverable errors: Access lost (resolution/mode change) or device removed
        if (metrics) {
            metrics->RecordCaptureError();
        }

        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL ||
            hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            std::cerr << "[WARN] DXGI recoverable error encountered: 0x" << std::hex << hr << std::dec << ". Attempting recovery...\n";
            return RecoverSession();
        }

        return false;
    }

    void ReleaseFrame() {
        if (hasAcquiredFrame && duplication) {
            duplication->ReleaseFrame();
            hasAcquiredFrame = false;
        }
    }

    bool RecoverSession() {
        ++consecutiveErrors;
        if (consecutiveErrors > MAX_CONSECUTIVE_ERRORS) {
            std::cerr << "[ERROR] Maximum consecutive capture recovery attempts exceeded.\n";
            Shutdown();
            if (metrics) metrics->RecordRecoveryAttempt(false);
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50U * consecutiveErrors));

        const bool ok = Initialize(config);
        if (metrics) {
            metrics->RecordRecoveryAttempt(ok);
        }
        return ok;
    }

    void Shutdown() {
        ReleaseFrame();
        retainedTexture.Reset();
        duplication.Reset();
        output1.Reset();
        adapter.Reset();
        factory.Reset();
        context.Reset();
        device.Reset();

        isInitialized = false;
        outputWidth = 0U;
        outputHeight = 0U;
    }
};

DxgiCapturer::DxgiCapturer(metrics::MetricsTracker* metrics)
    : pImpl_(std::make_unique<Impl>(metrics)) {}

DxgiCapturer::~DxgiCapturer() = default;

DxgiCapturer::DxgiCapturer(DxgiCapturer&&) noexcept = default;
DxgiCapturer& DxgiCapturer::operator=(DxgiCapturer&&) noexcept = default;

bool DxgiCapturer::Initialize(const config::EngineConfig& config) {
    return pImpl_->Initialize(config);
}

bool DxgiCapturer::AcquireFrame(uint32_t timeoutMs, CapturedFrame& outFrame) {
    return pImpl_->AcquireFrame(timeoutMs, outFrame);
}

void DxgiCapturer::ReleaseFrame() {
    pImpl_->ReleaseFrame();
}

void DxgiCapturer::Shutdown() {
    pImpl_->Shutdown();
}

bool DxgiCapturer::IsInitialized() const noexcept {
    return pImpl_->isInitialized;
}

uint32_t DxgiCapturer::GetOutputWidth() const noexcept {
    return pImpl_->outputWidth;
}

uint32_t DxgiCapturer::GetOutputHeight() const noexcept {
    return pImpl_->outputHeight;
}

int32_t DxgiCapturer::GetDesktopLeft() const noexcept {
    return pImpl_->desktopLeft;
}

int32_t DxgiCapturer::GetDesktopTop() const noexcept {
    return pImpl_->desktopTop;
}

uint32_t DxgiCapturer::GetDxgiFormat() const noexcept {
    return static_cast<uint32_t>(pImpl_->outputFormat);
}

const std::wstring& DxgiCapturer::GetDeviceName() const noexcept {
    return pImpl_->deviceName;
}

std::vector<MonitorInfo> DxgiCapturer::EnumerateOutputs() {
    std::vector<MonitorInfo> monitors;
    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        return monitors;
    }

    ComPtr<IDXGIAdapter1> curAdapter;
    uint32_t globalIdx = 0U;
    for (UINT aIdx = 0; factory->EnumAdapters1(aIdx, &curAdapter) != DXGI_ERROR_NOT_FOUND; ++aIdx) {
        ComPtr<IDXGIOutput> curOutput;
        for (UINT oIdx = 0; curAdapter->EnumOutputs(oIdx, &curOutput) != DXGI_ERROR_NOT_FOUND; ++oIdx) {
            DXGI_OUTPUT_DESC oDesc{};
            if (SUCCEEDED(curOutput->GetDesc(&oDesc)) && oDesc.AttachedToDesktop) {
                MonitorInfo info{};
                info.globalIndex = globalIdx++;
                info.deviceName = oDesc.DeviceName;
                info.left = oDesc.DesktopCoordinates.left;
                info.top = oDesc.DesktopCoordinates.top;
                info.width = static_cast<uint32_t>(oDesc.DesktopCoordinates.right - oDesc.DesktopCoordinates.left);
                info.height = static_cast<uint32_t>(oDesc.DesktopCoordinates.bottom - oDesc.DesktopCoordinates.top);
                info.isPrimary = (info.left == 0 && info.top == 0);
                monitors.push_back(std::move(info));
            }
        }
    }
    return monitors;
}

} // namespace secondscreen::capture
