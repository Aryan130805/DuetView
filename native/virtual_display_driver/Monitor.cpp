#include "Monitor.h"
#include <vector>

// 128-byte synthetic EDID for SecondScreen Virtual Monitor
// Declares: Manufacturer "SCS", Model "SecondScreen", Preferred Mode 1280x720 @ 60Hz, Secondary 1920x1080 @ 60Hz
static const BYTE s_VirtualMonitorEdid[128] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, // Header
    0x4C, 0xF3, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, // Manufacturer: "SCS", Product: 0x1001, Serial: 1
    0x2C, 0x20, 0x01, 0x04, 0xA5, 0x34, 0x1D, 0x78, // EDID 1.4, Digital input, 16:9 aspect
    0x22, 0xEE, 0x95, 0xA3, 0x54, 0x4C, 0x99, 0x26, // Color characteristics
    0x0F, 0x50, 0x54, 0xA5, 0x4B, 0x00, 0x81, 0x80, // Established timings
    0x81, 0xBC, 0x95, 0x00, 0xB3, 0x00, 0x01, 0x01, // Standard timings (1280x720, 1920x1080)
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    // Detailed Timing Descriptor 1: 1280x720p @ 60Hz (Preferred baseline)
    0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20,
    0x6E, 0x28, 0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00,
    0x00, 0x1E,
    // Detailed Timing Descriptor 2: 1920x1080p @ 60Hz
    0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
    0x58, 0x2C, 0x45, 0x00, 0xC4, 0x8E, 0x21, 0x00,
    0x00, 0x1E,
    // Descriptor 3: Display Range Limits (Vertical 50-75Hz, Horizontal 30-83kHz, Pixel Clock 170MHz)
    0x00, 0x00, 0x00, 0xFD, 0x00, 0x32, 0x4B, 0x1E,
    0x53, 0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20,
    // Descriptor 4: Display Name "SecondScreen"
    0x00, 0x00, 0x00, 0xFC, 0x00, 'S',  'e',  'c',
    'o',  'n',  'd',  'S',  'c',  'r',  'e',  'e',
    'n',  0x0A,
    0x00, 0x6E                                      // Extension flag = 0, Checksum = 0x6E
};

static DWORD CALLBACK SwapChainWorkerThread(LPVOID lpParam) {
    auto* pContext = static_cast<MonitorContext*>(lpParam);
    if (!pContext) return 0;

    while (!pContext->TerminateWorker) {
        DWORD waitRes = WaitForSingleObject(pContext->SwapChainEvent, 100);
        if (waitRes == WAIT_OBJECT_0) {
            IDARG_OUT_RELEASEANDACQUIREBUFFER outArgs{};
            HRESULT hr = IddCxSwapChainReleaseAndAcquireBuffer(pContext->IddSwapChain, &outArgs);

            if (SUCCEEDED(hr)) {
                // Buffer is acquired. In DXGI capture architecture,
                // Windows DWM mirrors or extends to IDXGIOutput where secondscreen_engine acquires it.
                IddCxSwapChainFinishedProcessingFrame(pContext->IddSwapChain);
            }
        }
    }

    return 0;
}

NTSTATUS CreateVirtualMonitor(_In_ IDDCX_ADAPTER AdapterObject) {
    IDDCX_MONITOR_INFO monitorInfo{};
    monitorInfo.Size = sizeof(monitorInfo);
    monitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED;
    monitorInfo.ConnectorIndex = 0;
    monitorInfo.MonitorDescription.Size = sizeof(monitorInfo.MonitorDescription);
    monitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    monitorInfo.MonitorDescription.DataSize = sizeof(s_VirtualMonitorEdid);
    monitorInfo.MonitorDescription.pData = const_cast<BYTE*>(s_VirtualMonitorEdid);

    // Generate unique stable container ID for this virtual monitor
    CoCreateGuid(&monitorInfo.MonitorContainerId);

    IDDCX_MONITOR monitorObject = WDF_NO_HANDLE;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, MonitorContext);

    IDARG_IN_MONITORCREATE inArgs{};
    inArgs.ObjectAttributes = &attributes;
    inArgs.pMonitorInfo = &monitorInfo;

    IDARG_OUT_MONITORCREATE outArgs{};
    NTSTATUS status = IddCxMonitorCreate(AdapterObject, &inArgs, &outArgs);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    monitorObject = outArgs.MonitorObject;

    MonitorContext* pContext = GetMonitorContext(monitorObject);
    pContext->IddMonitor = monitorObject;
    pContext->IddSwapChain = WDF_NO_HANDLE;
    pContext->SwapChainEvent = nullptr;
    pContext->WorkerThread = nullptr;
    pContext->TerminateWorker = false;

    // Notify Windows Plug and Play of monitor arrival
    IDARG_OUT_MONITORARRIVAL outArrival{};
    status = IddCxMonitorArrival(monitorObject, &outArrival);
    return status;
}

NTSTATUS EvtIddCxMonitorGetDefaultDescriptionModes(
    _In_ IDDCX_MONITOR MonitorObject,
    _In_ const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs,
    _Out_ IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);
    UNREFERENCED_PARAMETER(pInArgs);

    // Expose 1280x720 (conservative baseline) and 1920x1080 @ 60Hz
    pOutArgs->DefaultMonitorModeBufferOutputCount = 2;
    if (pInArgs->DefaultMonitorModeBufferInputCount >= 2) {
        // Mode 0: 1280 x 720 @ 60 Hz
        pInArgs->pDefaultMonitorModes[0].Size = sizeof(IDDCX_MONITOR_MODE);
        pInArgs->pDefaultMonitorModes[0].Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.totalSize.cx = 1650;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.totalSize.cy = 750;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.activeSize.cx = 1280;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.activeSize.cy = 720;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.vSyncFreq.Numerator = 60000;
        pInArgs->pDefaultMonitorModes[0].MonitorVideoSignalInfo.vSyncFreq.Denominator = 1000;

        // Mode 1: 1920 x 1080 @ 60 Hz
        pInArgs->pDefaultMonitorModes[1].Size = sizeof(IDDCX_MONITOR_MODE);
        pInArgs->pDefaultMonitorModes[1].Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.totalSize.cx = 2200;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.totalSize.cy = 1125;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.activeSize.cx = 1920;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.activeSize.cy = 1080;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.vSyncFreq.Numerator = 60000;
        pInArgs->pDefaultMonitorModes[1].MonitorVideoSignalInfo.vSyncFreq.Denominator = 1000;

        pOutArgs->PreferredMonitorModeIdx = 0; // Preferred baseline 1280x720
    }

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorQueryTargetModes(
    _In_ IDDCX_MONITOR MonitorObject,
    _In_ const IDARG_IN_QUERYTARGETMODES* pInArgs,
    _Out_ IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);

    pOutArgs->TargetModeBufferOutputCount = 2;
    if (pInArgs->TargetModeBufferInputCount >= 2) {
        // Mode 0: 1280 x 720 @ 60 Hz
        pInArgs->pTargetModes[0].Size = sizeof(IDDCX_TARGET_MODE);
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.totalSize.cx = 1650;
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.totalSize.cy = 750;
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.activeSize.cx = 1280;
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.activeSize.cy = 720;
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.vSyncFreq.Numerator = 60000;
        pInArgs->pTargetModes[0].TargetVideoSignalInfo.targetVideoSignalInfo.vSyncFreq.Denominator = 1000;

        // Mode 1: 1920 x 1080 @ 60 Hz
        pInArgs->pTargetModes[1].Size = sizeof(IDDCX_TARGET_MODE);
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.totalSize.cx = 2200;
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.totalSize.cy = 1125;
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.activeSize.cx = 1920;
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.activeSize.cy = 1080;
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.vSyncFreq.Numerator = 60000;
        pInArgs->pTargetModes[1].TargetVideoSignalInfo.targetVideoSignalInfo.vSyncFreq.Denominator = 1000;
    }

    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorAssignSwapChain(
    _In_ IDDCX_MONITOR MonitorObject,
    _In_ const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    MonitorContext* pContext = GetMonitorContext(MonitorObject);
    pContext->IddSwapChain = pInArgs->hSwapChain;
    pContext->SwapChainEvent = pInArgs->hNextSurfaceAvailable;
    pContext->TerminateWorker = false;

    // Launch worker thread to drain swapchain frames cleanly
    pContext->WorkerThread = CreateThread(nullptr, 0, SwapChainWorkerThread, pContext, 0, nullptr);
    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxMonitorUnassignSwapChain(
    _In_ IDDCX_MONITOR MonitorObject)
{
    MonitorContext* pContext = GetMonitorContext(MonitorObject);

    if (pContext->WorkerThread) {
        pContext->TerminateWorker = true;
        if (pContext->SwapChainEvent) {
            SetEvent(pContext->SwapChainEvent);
        }
        WaitForSingleObject(pContext->WorkerThread, 1000);
        CloseHandle(pContext->WorkerThread);
        pContext->WorkerThread = nullptr;
    }

    pContext->IddSwapChain = WDF_NO_HANDLE;
    pContext->SwapChainEvent = nullptr;
    return STATUS_SUCCESS;
}

