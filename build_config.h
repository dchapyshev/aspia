//
// PROJECT:         Aspia Remote Desktop
// FILE:            build_config.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BUILD_CONFIG_H
#define _ASPIA_BUILD_CONFIG_H

#include <stdint.h>

// Target version
#define _WIN32_WINNT     0x0601
#define NTDDI_VERSION    0x06010000 // Windows 7
#define _WIN32_IE        0x0800 // Internet Explorer 8.0
#define PSAPI_VERSION    2
#define WINVER           _WIN32_WINNT
#define _WIN32_WINDOWS   _WIN32_WINNT

static const uint16_t kDefaultHostTcpPort = 11011;

#endif // _ASPIA_BUILD_CONFIG_H
