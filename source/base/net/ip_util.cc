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

#include "base/net/ip_util.h"

#include "base/strings/unicode.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <WS2tcpip.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#else
#error Platform support not implemented
#endif // defined(OS_*)

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool ipv4FromString(std::u16string_view str, uint32_t* ip)
{
    struct sockaddr_in sa;
    if (inet_pton(AF_INET, base::local8BitFromUtf16(str).c_str(), &(sa.sin_addr)) != 1)
        return false;

#if defined(OS_WIN)
    *ip = sa.sin_addr.S_un.S_addr;
#else
    *ip = sa.sin_addr.s_addr;
#endif

    *ip = htonl(*ip);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool isValidIpV4Address(std::u16string_view address)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, base::local8BitFromUtf16(address).c_str(), &(sa.sin_addr)) != 0;
}

//--------------------------------------------------------------------------------------------------
bool isValidIpV6Address(std::u16string_view address)
{
    struct sockaddr_in6 sa;
    return inet_pton(AF_INET6, base::local8BitFromUtf16(address).c_str(), &(sa.sin6_addr)) != 0;
}

//--------------------------------------------------------------------------------------------------
bool isIpInRange(std::u16string_view ip, std::u16string_view network, std::u16string_view mask)
{
    uint32_t ip_addr = 0;
    if (!ipv4FromString(ip, &ip_addr))
        return false;

    uint32_t network_addr = 0;
    if (!ipv4FromString(network, &network_addr))
        return false;

    uint32_t mask_addr = 0;
    if (!ipv4FromString(mask, &mask_addr))
        return false;

    uint32_t lower_addr = (network_addr & mask_addr);
    uint32_t upper_addr = (lower_addr | (~mask_addr));

    if (ip_addr >= lower_addr && ip_addr <= upper_addr)
        return true;

    return false;
}

} // namespace base
