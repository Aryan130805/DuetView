#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wdf.h>
#include <iddcx.h>

#include "Trace.h"

extern "C" {

/// Standard WDF driver entry point.
DRIVER_INITIALIZE DriverEntry;

/// WDF device add callback triggered when Windows detects our PnP hardware ID.
EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;

/// WDF driver unload cleanup callback.
EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;

}

