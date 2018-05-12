//
// PROJECT:         Aspia
// FILE:            build_config.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BUILD_CONFIG_H
#define _ASPIA_BUILD_CONFIG_H

#define QT_DEPRECATED_WARNINGS
#define QT_MESSAGELOGCONTEXT

#include <QtGlobal>

#if defined(Q_OS_WIN)
// Set target version for MS Windows.
#define _WIN32_WINNT     0x0601
#define NTDDI_VERSION    0x06010000 // Windows 7
#define _WIN32_IE        0x0800 // Internet Explorer 8.0
#define PSAPI_VERSION    2
#define WINVER           _WIN32_WINNT
#define _WIN32_WINDOWS   _WIN32_WINNT
#endif // defined(Q_OS_WIN)

namespace aspia {

extern const int kDefaultHostTcpPort;

} // namespace

#endif // _ASPIA_BUILD_CONFIG_H
