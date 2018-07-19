//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_BUILD_CONFIG_H_
#define ASPIA_BUILD_CONFIG_H_

#define QT_DEPRECATED_WARNINGS
#define QT_MESSAGELOGCONTEXT
#define QT_USE_QSTRINGBUILDER

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

#endif // ASPIA_BUILD_CONFIG_H_
