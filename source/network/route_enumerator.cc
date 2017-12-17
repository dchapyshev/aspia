//
// PROJECT:         Aspia
// FILE:            network/route_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/route_enumerator.h"

#include <winsock2.h>

namespace aspia {

RouteEnumerator::RouteEnumerator()
{
    ULONG buffer_size = sizeof(MIB_IPFORWARDTABLE);

    forward_table_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
    forward_table_ = reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer_.get());

    DWORD error_code = GetIpForwardTable(forward_table_, &buffer_size, 0);
    if (error_code != NO_ERROR)
    {
        if (error_code == ERROR_INSUFFICIENT_BUFFER)
        {
            forward_table_buffer_ = std::make_unique<uint8_t[]>(buffer_size);
            forward_table_ = reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer_.get());

            error_code = GetIpForwardTable(forward_table_, &buffer_size, 0);
            if (error_code == NO_ERROR)
                return;
        }

        forward_table_buffer_.reset();
        forward_table_ = nullptr;
    }
}

bool RouteEnumerator::IsAtEnd() const
{
    if (!forward_table_)
        return true;

    return index_ >= forward_table_->dwNumEntries;
}

void RouteEnumerator::Advance()
{
    ++index_;
}

static std::string IpToString(DWORD ip)
{
    in_addr ip_address;
    ip_address.S_un.S_addr = ip;

    char* buffer = inet_ntoa(ip_address);
    if (!buffer)
        return std::string();

    return buffer;
}

std::string RouteEnumerator::GetDestonation() const
{
    return IpToString(forward_table_->table[index_].dwForwardDest);
}

std::string RouteEnumerator::GetMask() const
{
    return IpToString(forward_table_->table[index_].dwForwardMask);
}

std::string RouteEnumerator::GetGateway() const
{
    return IpToString(forward_table_->table[index_].dwForwardNextHop);
}

uint32_t RouteEnumerator::GetMetric() const
{
    return forward_table_->table[index_].dwForwardMetric1;
}

} // namespace aspia
