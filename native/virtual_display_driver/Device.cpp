#include "Device.h"
#include "Monitor.h"

NTSTATUS SecondScreenDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT pDeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);

    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDeviceD0Entry = EvtDeviceD0Entry;
    pnpCallbacks.EvtDeviceD0Exit = EvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DeviceContext);

    WDFDEVICE device = WDF_NO_HANDLE;
    NTSTATUS status = WdfDeviceCreate(&pDeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    DeviceContext* pContext = GetDeviceContext(device);
    pContext->WdfDevice = device;
    pContext->IddAdapter = WDF_NO_HANDLE;

    // Prepare IddCx adapter creation callbacks
    IDDCX_ADAPTER_CAPS adapterCaps{};
    adapterCaps.Size = sizeof(adapterCaps);
    adapterCaps.Flags = IDDCX_ADAPTER_FLAGS_CAN_USE_MOVE_REGIONS;
    adapterCaps.MaxMonitorsSupported = 1;
    adapterCaps.EndPointDiagnostics.Size = sizeof(adapterCaps.EndPointDiagnostics);
    adapterCaps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    adapterCaps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    IDARG_IN_ADAPTER_INIT inArgs{};
    inArgs.WdfDevice = device;
    inArgs.pCaps = &adapterCaps;
    inArgs.ObjectAttributes = nullptr;

    IDARG_OUT_ADAPTER_INIT outArgs{};
    status = IddCxAdapterInitAsync(&inArgs, &outArgs);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    pContext->IddAdapter = outArgs.AdapterObject;
    return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(PreviousState);
    return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);
    return STATUS_SUCCESS;
}

NTSTATUS EvtIddCxAdapterInitFinished(
    _In_ IDDCX_ADAPTER AdapterObject,
    _In_ const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
    if (!NT_SUCCESS(pInArgs->AdapterInitStatus)) {
        return pInArgs->AdapterInitStatus;
    }

    // Now that adapter is initialized by Windows, create and attach virtual monitor
    return CreateVirtualMonitor(AdapterObject);
}

NTSTATUS EvtIddCxAdapterCommitModes(
    _In_ IDDCX_ADAPTER AdapterObject,
    _In_ const IDARG_IN_COMMITMODES* pInArgs)
{
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);
    return STATUS_SUCCESS;
}

