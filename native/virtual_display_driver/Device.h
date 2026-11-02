#pragma once

#include "Driver.h"

struct DeviceContext {
    WDFDEVICE WdfDevice;
    IDDCX_ADAPTER IddAdapter;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DeviceContext, GetDeviceContext);

/// Creates the WDF device and initiates IddCx adapter creation.
NTSTATUS SecondScreenDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT pDeviceInit);

// IddCx Adapter Callbacks
EVT_IDD_CX_ADAPTER_INIT_FINISHED EvtIddCxAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES EvtIddCxAdapterCommitModes;
EVT_WDF_DEVICE_D0_ENTRY EvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT EvtDeviceD0Exit;

