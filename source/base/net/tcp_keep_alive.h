//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_TCP_KEEP_ALIVE_H
#define BASE_NET_TCP_KEEP_ALIVE_H

#include "build/build_config.h"

#include <chrono>
#include <cstdint>

namespace base {

#if defined(OS_WIN)
using NativeSocket = uintptr_t;
#elif defined(OS_POSIX)
using NativeSocket = int;
#else
#error Not implemented
#endif

bool setTcpKeepAlive(NativeSocket socket,
                     bool enable,
                     const std::chrono::milliseconds& time,
                     const std::chrono::milliseconds& interval);

} // namespace base

#endif // BASE_NET_TCP_KEEP_ALIVE_H
