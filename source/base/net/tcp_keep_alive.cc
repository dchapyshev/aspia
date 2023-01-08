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

#include "base/net/tcp_keep_alive.h"

#include "base/logging.h"

#if defined(OS_WIN)
#include <WinSock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

#if defined(OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif // defined(OS_POSIX)

namespace base {

bool setTcpKeepAlive(NativeSocket socket,
                     bool enable,
                     const std::chrono::milliseconds& time,
                     const std::chrono::milliseconds& interval)
{
#if defined(OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = enable ? TRUE : FALSE;
    alive.keepalivetime = static_cast<ULONG>(time.count());
    alive.keepaliveinterval = static_cast<ULONG>(interval.count());

    DWORD bytes_returned = 0;

    if (WSAIoctl(socket, SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        PLOG(LS_WARNING) << "WSAIoctl failed";
        return false;
    }

    return true;
#elif defined(OS_POSIX)
    int yes = enable ? 1 : 0;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) == -1)
    {
        PLOG(LS_WARNING) << "setsockopt(SO_KEEPALIVE) failed";
        return false;
    }

    if (!enable)
        return true;

    int idle = std::chrono::duration_cast<std::chrono::seconds>(time).count();
#if defined(OS_LINUX)
    int option_name = TCP_KEEPIDLE;
#elif defined(OS_MAC)
    int option_name = TCP_KEEPALIVE;
#endif
    if (setsockopt(socket, IPPROTO_TCP, option_name, &idle, sizeof(int)) == -1)
    {
        PLOG(LS_WARNING) << "setsockopt(TCP_KEEPIDLE) failed";
        return false;
    }

    int ival = std::chrono::duration_cast<std::chrono::seconds>(interval).count();
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &ival, sizeof(int)) == -1)
    {
        PLOG(LS_WARNING) << "setsockopt(TCP_KEEPINTVL) failed";
        return false;
    }

    int maxpkt = 10;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) == -1)
    {
        PLOG(LS_WARNING) << "setsockopt(TCP_KEEPCNT) failed";
        return false;
    }

    return true;
#else
    #warning Not implemented
    return false;
#endif
}

} // namespace base
