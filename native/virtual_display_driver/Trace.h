#pragma once

#include <windows.h>
#include <wdf.h>

// WPP Software Tracing Definitions
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID( \
        SecondScreenIddTraceGuid, (9A65D397,8B0D,4F2A,806F,56E81A5D3302), \
        WPP_DEFINE_BIT(TRACE_DRIVER) \
        WPP_DEFINE_BIT(TRACE_DEVICE) \
        WPP_DEFINE_BIT(TRACE_MONITOR) \
        WPP_DEFINE_BIT(TRACE_SWAPCHAIN) \
    )

#define WPP_FLAG_LEVEL_LOGGER(flag, level) \
    WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level) \
    (WPP_LEVEL_ENABLED(flag) && WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define TRACE_LEVEL_CRITICAL 1
#define TRACE_LEVEL_ERROR    2
#define TRACE_LEVEL_WARNING  3
#define TRACE_LEVEL_INFO     4
#define TRACE_LEVEL_VERBOSE  5

