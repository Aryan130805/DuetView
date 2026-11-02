#pragma once

#include "Driver.h"

struct MonitorContext {
    IDDCX_MONITOR IddMonitor;
    IDDCX_SWAPCHAIN IddSwapChain;
    HANDLE SwapChainEvent;
    HANDLE WorkerThread;
    bool TerminateWorker;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MonitorContext, GetMonitorContext);

/// Creates and registers the virtual monitor with Windows IddCx.
NTSTATUS CreateVirtualMonitor(_In_ IDDCX_ADAPTER AdapterObject);

// Monitor Callbacks
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES EvtIddCxMonitorGetDefaultDescriptionModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES EvtIddCxMonitorQueryTargetModes;
EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN EvtIddCxMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN EvtIddCxMonitorUnassignSwapChain;

