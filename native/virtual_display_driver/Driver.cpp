#include "Driver.h"
#include "Device.h"

extern "C" NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT pDriverObject,
    _In_ PUNICODE_STRING pRegistryPath)
{
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, EvtDriverDeviceAdd);
    config.EvtDriverUnload = EvtDriverUnload;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    NTSTATUS status = WdfDriverCreate(
        pDriverObject,
        pRegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

extern "C" NTSTATUS EvtDriverDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT pDeviceInit)
{
    NTSTATUS status = STATUS_SUCCESS;

    // Configure IddCx device initialization
    status = IddCxDeviceInitConfig(pDeviceInit, nullptr);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Hand over device creation and adapter initialization to Device component
    status = SecondScreenDeviceAdd(Driver, pDeviceInit);
    return status;
}

extern "C" VOID EvtDriverUnload(
    _In_ WDFDRIVER Driver)
{
    UNREFERENCED_PARAMETER(Driver);
}

