#include "encoder/mf_h264_encoder.hpp"
#include "encoder/h264_parser.hpp"
#include "capture/cursor_manager.hpp"

#include <windows.h>
#include <strmif.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <codecapi.h>
#include <wrl/client.h>

#ifndef MF_E_NOTACCEPTING
#define MF_E_NOTACCEPTING ((HRESULT)0xC00D36B5L)
#endif

#ifndef MF_E_TRANSFORM_NEED_MORE_INPUT
#define MF_E_TRANSFORM_NEED_MORE_INPUT ((HRESULT)0xC00D6D72L)
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace secondscreen::encoder {

using Microsoft::WRL::ComPtr;

// Intel Quick Sync Video H.264 Encoder MFT: {4BE8D3C0-0515-4A37-AD55-E4BAE19AF471}
static const GUID CLSID_IntelQSVH264Enc =
    {0x4be8d3c0, 0x0515, 0x4a37, {0xad, 0x55, 0xe4, 0xba, 0xe1, 0x9a, 0xf4, 0x71}};

// Microsoft In-box H.264 Encoder MFT: {6CA50344-051A-4DED-9779-A43305165E35}
static const GUID CLSID_MSH264Enc =
    {0x6ca50344, 0x051a, 0x4ded, {0x97, 0x79, 0xa4, 0x33, 0x05, 0x16, 0x5e, 0x35}};

static const GUID IID_ICodecAPI_Local =
    {0x901d859e, 0xaca7, 0x4c75, {0xb6, 0xd8, 0x9f, 0x20, 0xf0, 0x89, 0x60, 0x81}};

#ifndef CODECAPI_AVEncCommonMeanBitRate
static const GUID CODECAPI_AVEncCommonMeanBitRate =
    {0x2034ff98, 0x56a1, 0x476b, {0x94, 0x4f, 0xc4, 0x19, 0xc8, 0xb7, 0x4a, 0x44}};
#endif

#ifndef CODECAPI_AVEncCommonQualityVsSpeed
static const GUID CODECAPI_AVEncCommonQualityVsSpeed =
    {0x9822a161, 0x8c0d, 0x4c4e, {0x90, 0x04, 0x2b, 0xf3, 0x8e, 0x50, 0x0c, 0x66}};
#endif

#ifndef CODECAPI_AVEncMPVDefaultBPictureCount
static const GUID CODECAPI_AVEncMPVDefaultBPictureCount =
    {0x706275c4, 0xe7c7, 0x45ec, {0xa0, 0x27, 0x3a, 0x1c, 0x99, 0xf8, 0x42, 0x56}};
#endif

#ifndef CODECAPI_AVEncNumWorkerThreads
static const GUID CODECAPI_AVEncNumWorkerThreads =
    {0x256434fa, 0xdd45, 0x4e28, {0x92, 0xe7, 0x19, 0x9b, 0xe4, 0x67, 0x5f, 0x70}};
#endif

// Parallel row-slice BGRA -> NV12 color conversion helper
static void ConvertBgraToNv12Slice(const uint8_t* bgra, uint32_t width, uint32_t stride,
                                   uint8_t* yPlane, uint8_t* uvPlane,
                                   uint32_t startRow, uint32_t endRow) noexcept {
    const uint32_t halfWidth = width / 2;

    for (uint32_t r = startRow; r < endRow; ++r) {
        const uint8_t* rowBgra = bgra + r * stride;
        uint8_t* rowY = yPlane + r * width;
        for (uint32_t c = 0; c < width; ++c) {
            const int32_t b = rowBgra[c * 4];
            const int32_t g = rowBgra[c * 4 + 1];
            const int32_t red = rowBgra[c * 4 + 2];
            const int32_t y = ((66 * red + 129 * g + 25 * b + 128) >> 8) + 16;
            rowY[c] = static_cast<uint8_t>(std::clamp(y, 16, 235));
        }

        if ((r % 2) == 0) {
            const uint32_t uvRow = r / 2;
            uint8_t* rowUv = uvPlane + uvRow * width;
            for (uint32_t c = 0; c < halfWidth; ++c) {
                const int32_t b = rowBgra[(c * 2) * 4];
                const int32_t g = rowBgra[(c * 2) * 4 + 1];
                const int32_t red = rowBgra[(c * 2) * 4 + 2];

                const int32_t u = ((-38 * red - 74 * g + 112 * b + 128) >> 8) + 128;
                const int32_t v = ((112 * red - 94 * g - 18 * b + 128) >> 8) + 128;

                rowUv[c * 2] = static_cast<uint8_t>(std::clamp(u, 16, 240));
                rowUv[c * 2 + 1] = static_cast<uint8_t>(std::clamp(v, 16, 240));
            }
        }
    }
}

void ConvertBgraToNv12(const uint8_t* bgra, uint32_t width, uint32_t height, uint32_t stride,
                       uint8_t* yPlane, uint8_t* uvPlane) noexcept {
    const unsigned int numThreads = std::min(4u, std::max(1u, std::thread::hardware_concurrency()));
    if (numThreads <= 1 || height < 128) {
        ConvertBgraToNv12Slice(bgra, width, stride, yPlane, uvPlane, 0, height);
        return;
    }

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    const uint32_t rowsPerThread = (height / numThreads) & ~1u;

    for (unsigned int i = 0; i < numThreads; ++i) {
        const uint32_t startRow = i * rowsPerThread;
        const uint32_t endRow = (i == numThreads - 1) ? height : startRow + rowsPerThread;
        workers.emplace_back(ConvertBgraToNv12Slice, bgra, width, stride, yPlane, uvPlane, startRow, endRow);
    }

    for (auto& w : workers) {
        w.join();
    }
}

struct MfH264Encoder::Impl {
    metrics::MetricsTracker* metrics{nullptr};
    config::EngineConfig config{};
    EncodedOutputCallback callback{};

    ComPtr<IMFTransform> transform{};
    ComPtr<ICodecAPI> codecApi{};
    ComPtr<ID3D11Device> stagingDevice{};
    ComPtr<ID3D11DeviceContext> stagingContext{};
    ComPtr<ID3D11Texture2D> stagingTexture{};

    std::string encoderName{};
    bool isHardware{false};
    bool isInitialized{false};
    bool hasExtractedConfig{false};
    std::vector<uint8_t> cachedCodecConfig{};

    uint32_t width{0U};
    uint32_t height{0U};
    uint32_t targetFps{30U};
    uint32_t bitrateKbps{6000U};
    uint64_t sampleCount{0U};
    uint32_t sequenceNumber{0U};

    capture::CursorManager cursorManager{};
    std::vector<uint8_t> nv12Buffer{};

    explicit Impl(metrics::MetricsTracker* m) : metrics(m) {}

    ~Impl() {
        Shutdown();
    }

    bool Initialize(const config::EngineConfig& cfg, const EncodedOutputCallback& cb) {
        Shutdown();
        config = cfg;
        callback = cb;

        width = config.width;
        height = config.height;
        targetFps = config.targetFps;
        bitrateKbps = config.bitrateKbps;

        // Allocate NV12 staging buffer
        const size_t ySize = width * height;
        const size_t uvSize = width * (height / 2);
        nv12Buffer.resize(ySize + uvSize, 0x80);

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        (void)hr;
        hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] MFStartup failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        // 1. Try Hardware Encoder first if preferred
        bool initialized = false;
        if (config.preferHardwareEncoder) {
            hr = CoCreateInstance(CLSID_IntelQSVH264Enc, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IMFTransform, (void**)&transform);
            if (SUCCEEDED(hr) && transform) {
                ComPtr<IMFAttributes> attrs;
                UINT32 isAsync = 0;
                if (SUCCEEDED(transform->GetAttributes(&attrs))) {
                    attrs->GetUINT32(MF_TRANSFORM_ASYNC, &isAsync);
                }

                if (isAsync) {
                    // Intel QSV MFT is asynchronous (MF_TRANSFORM_ASYNC=1). On this Windows platform,
                    // asynchronous hardware transforms require an asynchronous Media Session topology and do not
                    // emit output in standalone synchronous pipelines. Fall back to Microsoft In-box encoder.
                    std::cout << "[INFO] Preferred Intel Quick Sync Video MFT is asynchronous (MF_TRANSFORM_ASYNC=1).\n"
                              << "[INFO] Falling back to Microsoft In-box H.264 Encoder MFT for synchronous streaming.\n";
                    transform.Reset();
                } else if (ConfigureMediaTypes()) {
                    isHardware = true;
                    encoderName = "Intel(R) Quick Sync Video H.264 Hardware Encoder MFT";
                    initialized = true;
                    std::cout << "[INFO] Selected Hardware Encoder: " << encoderName << "\n";
                } else {
                    transform.Reset();
                }
            }
        }

        // 2. Fallback to Microsoft In-box H.264 MFT
        if (!initialized) {
            std::cout << "[INFO] Instantiating Microsoft In-box H.264 Encoder MFT fallback...\n";
            hr = CoCreateInstance(CLSID_MSH264Enc, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IMFTransform, (void**)&transform);
            if (FAILED(hr) || !transform) {
                std::cerr << "[ERROR] Failed to instantiate any H.264 encoder MFT: 0x"
                          << std::hex << hr << std::dec << "\n";
                return false;
            }

            if (!ConfigureMediaTypes()) {
                std::cerr << "[ERROR] Failed to negotiate media types on Microsoft H.264 MFT.\n";
                transform.Reset();
                return false;
            }

            isHardware = false;
            encoderName = "Microsoft H.264 Encoder MFT (Software Fallback)";
            initialized = true;
            std::cout << "[INFO] Selected Fallback Encoder: " << encoderName << "\n";
        }

        // 3. Configure Low-Latency and GOP settings via ICodecAPI
        hr = transform->QueryInterface(IID_ICodecAPI_Local, (void**)&codecApi);
        if (SUCCEEDED(hr) && codecApi) {
            VARIANT var;
            VariantInit(&var);

            // Enable low latency mode
            var.vt = VT_BOOL;
            var.boolVal = VARIANT_TRUE;
            codecApi->SetValue(&CODECAPI_AVLowLatencyMode, &var);

            // Set keyframe interval (GOP size)
            var.vt = VT_UI4;
            var.ulVal = config.keyframeIntervalFrames;
            codecApi->SetValue(&CODECAPI_AVEncMPVGOPSize, &var);

            // Disable B-pictures completely for zero buffering/reordering delay
            var.vt = VT_UI4;
            var.ulVal = 0;
            codecApi->SetValue(&CODECAPI_AVEncMPVDefaultBPictureCount, &var);

            // Configure QualityVsSpeed to 0 (Fastest speed / ultra-low latency)
            var.vt = VT_UI4;
            var.ulVal = 0;
            codecApi->SetValue(&CODECAPI_AVEncCommonQualityVsSpeed, &var);

            // Configure worker threads for multi-threaded encoding
            var.vt = VT_UI4;
            var.ulVal = std::min(4u, std::max(1u, std::thread::hardware_concurrency()));
            codecApi->SetValue(&CODECAPI_AVEncNumWorkerThreads, &var);
        }

        // 4. Begin streaming notifications
        transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

        isInitialized = true;
        sampleCount = 0U;
        sequenceNumber = 0U;
        hasExtractedConfig = false;
        cachedCodecConfig.clear();

        return true;
    }

    bool ConfigureMediaTypes() {
        if (!transform) return false;

        // Output Type: H.264
        ComPtr<IMFMediaType> outType;
        HRESULT hr = MFCreateMediaType(&outType);
        if (FAILED(hr)) return false;

        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outType->SetUINT32(MF_MT_AVG_BITRATE, bitrateKbps * 1000U);
        MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, targetFps, 1);
        outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        outType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);

        hr = transform->SetOutputType(0, outType.Get(), 0);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] SetOutputType H264 failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        // Input Type: NV12
        ComPtr<IMFMediaType> inType;
        hr = MFCreateMediaType(&inType);
        if (FAILED(hr)) return false;

        inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, width, height);
        MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, targetFps, 1);
        inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

        hr = transform->SetInputType(0, inType.Get(), 0);
        if (FAILED(hr)) {
            std::cerr << "[ERROR] SetInputType NV12 failed: 0x" << std::hex << hr << std::dec << "\n";
            return false;
        }

        return true;
    }

    bool EnsureStagingTexture(ID3D11Device* dev, uint32_t texW, uint32_t texH) {
        if (stagingTexture && stagingDevice.Get() == dev) {
            return true;
        }

        stagingDevice = dev;
        stagingDevice->GetImmediateContext(&stagingContext);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = texW;
        desc.Height = texH;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = stagingDevice->CreateTexture2D(&desc, nullptr, &stagingTexture);
        return SUCCEEDED(hr);
    }

    bool Encode(const capture::CapturedFrame& frame) {
        if (!isInitialized || !transform) {
            return false;
        }

        const auto encodeStart = std::chrono::steady_clock::now();

        // 1. Process texture data into NV12
        if (frame.d3d11Texture) {
            auto* d3dTex = static_cast<ID3D11Texture2D*>(frame.d3d11Texture);
            ComPtr<ID3D11Device> texDev;
            d3dTex->GetDevice(&texDev);

            if (EnsureStagingTexture(texDev.Get(), frame.width, frame.height)) {
                stagingContext->CopyResource(stagingTexture.Get(), d3dTex);

                D3D11_MAPPED_SUBRESOURCE mapped{};
                HRESULT hr = stagingContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mapped);
                if (SUCCEEDED(hr)) {
                    const uint32_t activeW = std::min(width, frame.width);
                    const uint32_t activeH = std::min(height, frame.height);
                    auto* pBgra = static_cast<uint8_t*>(mapped.pData);

                    // Composite active hardware cursor onto video frame before NV12 encoding
                    cursorManager.CompositeCursor(pBgra, activeW, activeH, mapped.RowPitch);

                    uint8_t* yPlane = nv12Buffer.data();
                    uint8_t* uvPlane = yPlane + (width * height);

                    ConvertBgraToNv12(pBgra,
                                      activeW,
                                      activeH,
                                      mapped.RowPitch,
                                      yPlane,
                                      uvPlane);

                    stagingContext->Unmap(stagingTexture.Get(), 0);
                }
            }
        }

        // 2. Submit NV12 sample to encoder
        ComPtr<IMFMediaBuffer> inBuffer;
        HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(nv12Buffer.size()), &inBuffer);
        if (FAILED(hr)) return false;

        BYTE* pDst = nullptr;
        inBuffer->Lock(&pDst, nullptr, nullptr);
        std::memcpy(pDst, nv12Buffer.data(), nv12Buffer.size());
        inBuffer->Unlock();
        inBuffer->SetCurrentLength(static_cast<DWORD>(nv12Buffer.size()));

        ComPtr<IMFSample> inSample;
        hr = MFCreateSample(&inSample);
        if (FAILED(hr)) return false;

        inSample->AddBuffer(inBuffer.Get());
        const LONGLONG sampleTimeHns = static_cast<LONGLONG>(sampleCount * (10000000ULL / targetFps));
        const LONGLONG sampleDurationHns = static_cast<LONGLONG>(10000000ULL / targetFps);
        inSample->SetSampleTime(sampleTimeHns);
        inSample->SetSampleDuration(sampleDurationHns);
        ++sampleCount;

        hr = transform->ProcessInput(0, inSample.Get(), 0);
        if (FAILED(hr)) {
            static int inErrCount = 0;
            if (inErrCount++ < 5) {
                std::cerr << "[DEBUG] ProcessInput returned: 0x" << std::hex << hr << std::dec << "\n";
            }
            if (hr != MF_E_NOTACCEPTING) {
                return false;
            }
        }

        // 3. Drain output samples
        DrainOutputs(encodeStart, frame.timestampUs);
        return true;
    }

    void DrainOutputs(const std::chrono::steady_clock::time_point& start, uint64_t fallbackTimestampUs) {
        MFT_OUTPUT_STREAM_INFO osInfo{};
        HRESULT hrInfo = transform->GetOutputStreamInfo(0, &osInfo);
        const bool mftProvidesSamples = SUCCEEDED(hrInfo) &&
            ((osInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0);
        const DWORD minBufferSize = (SUCCEEDED(hrInfo) && osInfo.cbSize > 0) ? osInfo.cbSize : (2 * 1024 * 1024);

        while (true) {
            MFT_OUTPUT_DATA_BUFFER outputBuffer{};
            outputBuffer.dwStreamID = 0;

            ComPtr<IMFMediaBuffer> clientBuf;
            ComPtr<IMFSample> clientSample;

            if (!mftProvidesSamples) {
                HRESULT hrBuf = MFCreateMemoryBuffer(minBufferSize, &clientBuf);
                if (FAILED(hrBuf)) break;
                HRESULT hrSmpl = MFCreateSample(&clientSample);
                if (FAILED(hrSmpl)) break;
                clientSample->AddBuffer(clientBuf.Get());
                outputBuffer.pSample = clientSample.Get();
            } else {
                outputBuffer.pSample = nullptr;
            }

            DWORD status = 0;
            HRESULT hr = transform->ProcessOutput(0, 1, &outputBuffer, &status);
            if (FAILED(hr)) {
                break;
            }

            IMFSample* producedSample = outputBuffer.pSample ? outputBuffer.pSample : clientSample.Get();
            if (producedSample) {
                ComPtr<IMFMediaBuffer> contiguous;
                hr = producedSample->ConvertToContiguousBuffer(&contiguous);
                if (SUCCEEDED(hr)) {
                    BYTE* pData = nullptr;
                    DWORD len = 0;
                    contiguous->Lock(&pData, nullptr, &len);

                    if (len > 0) {
                        const std::span<const uint8_t> bitstream(pData, len);

                        const auto encodeDurUs = static_cast<uint64_t>(
                            std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - start
                            ).count()
                        );

                        // 1. Check if SPS/PPS codec configuration is present
                        std::vector<uint8_t> spsPps;
                        if (H264Parser::ExtractSpsPps(bitstream, spsPps)) {
                            if (!hasExtractedConfig || spsPps != cachedCodecConfig) {
                                hasExtractedConfig = true;
                                cachedCodecConfig = spsPps;

                                if (callback) {
                                    EncodedPacket cfgPkt{};
                                    cfgPkt.type = protocol::PacketType::Config;
                                    cfgPkt.timestampUs = fallbackTimestampUs;
                                    cfgPkt.data = spsPps;
                                    cfgPkt.isKeyframe = true;
                                    cfgPkt.encodeDurationUs = encodeDurUs;
                                    cfgPkt.sequenceNumber = ++sequenceNumber;
                                    callback(cfgPkt);
                                }
                            }
                        }

                        // 2. Classify and emit video access unit (IDR vs Delta)
                        const bool isKey = H264Parser::ContainsIdrSlice(bitstream);

                        if (callback) {
                            EncodedPacket videoPkt{};
                            videoPkt.type = isKey ? protocol::PacketType::Keyframe : protocol::PacketType::Delta;
                            videoPkt.timestampUs = fallbackTimestampUs;
                            videoPkt.data.assign(pData, pData + len);
                            videoPkt.isKeyframe = isKey;
                            videoPkt.encodeDurationUs = encodeDurUs;
                            videoPkt.sequenceNumber = ++sequenceNumber;
                            callback(videoPkt);
                        }

                        if (metrics) {
                            metrics->RecordFrameEncoded(len, encodeDurUs);
                        }
                    }

                    contiguous->Unlock();
                }

                if (mftProvidesSamples && outputBuffer.pSample) {
                    outputBuffer.pSample->Release();
                }
            }
        }
    }

    void RequestKeyframe() {
        if (codecApi) {
            VARIANT var;
            VariantInit(&var);
            var.vt = VT_UI4;
            var.ulVal = 1;
            codecApi->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
        }
    }

    void Flush() {
        if (transform) {
            transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
            transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
            for (int drainPass = 0; drainPass < 30; ++drainPass) {
                DrainOutputs(std::chrono::steady_clock::now(), 0ULL);
            }
            transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
        }
    }

    void Shutdown() {
        Flush();
        transform.Reset();
        codecApi.Reset();
        stagingTexture.Reset();
        stagingContext.Reset();
        stagingDevice.Reset();

        isInitialized = false;
        hasExtractedConfig = false;
        cachedCodecConfig.clear();
        nv12Buffer.clear();
        MFShutdown();
    }

    bool SetBitrate(uint32_t newBitrateKbps) {
        if (newBitrateKbps == 0) return false;
        bitrateKbps = newBitrateKbps;

        if (codecApi) {
            VARIANT var;
            VariantInit(&var);
            var.vt = VT_UI4;
            var.ulVal = newBitrateKbps * 1000U;
            const HRESULT hr = codecApi->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
            if (SUCCEEDED(hr)) {
                std::cout << "[INFO] Dynamically updated encoder bitrate to " << newBitrateKbps << " Kbps via ICodecAPI\n";
                return true;
            }
        }
        return false;
    }

    bool SetTargetFps(uint32_t newTargetFps) {
        if (newTargetFps == 0) return false;
        targetFps = newTargetFps;
        return true;
    }
};

MfH264Encoder::MfH264Encoder(metrics::MetricsTracker* metrics)
    : pImpl_(std::make_unique<Impl>(metrics)) {}

MfH264Encoder::~MfH264Encoder() = default;

MfH264Encoder::MfH264Encoder(MfH264Encoder&&) noexcept = default;
MfH264Encoder& MfH264Encoder::operator=(MfH264Encoder&&) noexcept = default;

bool MfH264Encoder::Initialize(const config::EngineConfig& config,
                              const EncodedOutputCallback& callback) {
    return pImpl_->Initialize(config, callback);
}

bool MfH264Encoder::Encode(const capture::CapturedFrame& frame) {
    return pImpl_->Encode(frame);
}

void MfH264Encoder::RequestKeyframe() {
    pImpl_->RequestKeyframe();
}

bool MfH264Encoder::SetBitrate(uint32_t bitrateKbps) {
    return pImpl_->SetBitrate(bitrateKbps);
}

bool MfH264Encoder::SetTargetFps(uint32_t targetFps) {
    return pImpl_->SetTargetFps(targetFps);
}

void MfH264Encoder::Flush() {
    pImpl_->Flush();
}

void MfH264Encoder::Shutdown() {
    pImpl_->Shutdown();
}

bool MfH264Encoder::IsHardwareAccelerated() const noexcept {
    return pImpl_->isHardware;
}

const std::string& MfH264Encoder::GetEncoderName() const noexcept {
    return pImpl_->encoderName;
}

bool MfH264Encoder::HasExtractedConfig() const noexcept {
    return pImpl_->hasExtractedConfig;
}

const std::vector<uint8_t>& MfH264Encoder::GetCodecConfig() const noexcept {
    return pImpl_->cachedCodecConfig;
}

} // namespace secondscreen::encoder
