//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/connect_enumerator.h"

#include <QtEndian>

#include <iphlpapi.h>
#include <TlHelp32.h>
#include <WinSock2.h>

#include "base/logging.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QByteArray initializeTcpTable()
{
    ULONG table_buffer_size = sizeof(MIB_TCPTABLE);

    QByteArray table_buffer;
    table_buffer.resize(table_buffer_size);

    DWORD ret = GetExtendedTcpTable(reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer.data()),
                                    &table_buffer_size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer.resize(table_buffer_size);

        ret = GetExtendedTcpTable(reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer.data()),
                                  &table_buffer_size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    }

    if (ret != NO_ERROR)
        return nullptr;

    return table_buffer;
}

//--------------------------------------------------------------------------------------------------
QByteArray initializeUdpTable()
{
    ULONG table_buffer_size = sizeof(MIB_UDPTABLE);

    QByteArray table_buffer;
    table_buffer.resize(table_buffer_size);

    DWORD ret = GetExtendedUdpTable(reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer.data()),
                                    &table_buffer_size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer.resize(table_buffer_size);

        ret = GetExtendedUdpTable(reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer.data()),
                                  &table_buffer_size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    }

    if (ret != NO_ERROR)
        return nullptr;

    return table_buffer;
}

//--------------------------------------------------------------------------------------------------
QString processNameByPid(HANDLE process_snapshot, DWORD process_id)
{
    PROCESSENTRY32W process_entry;
    process_entry.dwSize = sizeof(process_entry);

    if (Process32FirstW(process_snapshot, &process_entry))
    {
        do
        {
            if (process_entry.th32ProcessID == process_id)
                return QString::fromWCharArray(process_entry.szExeFile);
        } while (Process32NextW(process_snapshot, &process_entry));
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
QString addressToString(quint32 address)
{
    address = qbswap(address);

    return QString("%1.%2.%3.%4")
        .arg((address >> 24) & 0xFF)
        .arg((address >> 16) & 0xFF)
        .arg((address >> 8)  & 0xFF)
        .arg(address & 0xFF);
}

//--------------------------------------------------------------------------------------------------
QString stateToString(DWORD state)
{
    switch (state)
    {
        case 0:  return "UNKNOWN";
        case 1:  return "CLOSED";
        case 2:  return "LISTENING";
        case 3:  return "SYN_SENT";
        case 4:  return "SYN_RCVD";
        case 5:  return "ESTABLISHED";
        case 6:  return "FIN_WAIT1";
        case 7:  return "FIN_WAIT2";
        case 8:  return "CLOSE_WAIT";
        case 9:  return "CLOSING";
        case 10: return "LAST_ACK";
        case 11: return "TIME_WAIT";
        case 12: return "DELETE_TCB";
        default: return "UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ConnectEnumerator::ConnectEnumerator(Mode mode)
    : mode_(mode),
      snapshot_(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0))
{
    if (mode_ == Mode::TCP)
    {
        table_buffer_ = initializeTcpTable();

        PMIB_TCPTABLE_OWNER_PID tcp_table =
            reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer_.data());
        if (tcp_table)
            num_entries_ = tcp_table->dwNumEntries;
    }
    else
    {
        DCHECK_EQ(mode_, Mode::UDP);
        table_buffer_ = initializeUdpTable();

        PMIB_UDPTABLE_OWNER_PID udp_table =
            reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer_.data());
        if (udp_table)
            num_entries_ = udp_table->dwNumEntries;
    }
}

//--------------------------------------------------------------------------------------------------
ConnectEnumerator::~ConnectEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool ConnectEnumerator::isAtEnd() const
{
    return pos_ >= num_entries_;
}

//--------------------------------------------------------------------------------------------------
void ConnectEnumerator::advance()
{
    ++pos_;
}

//--------------------------------------------------------------------------------------------------
QString ConnectEnumerator::protocol() const
{
    return (mode_ == Mode::TCP) ? "TCP" : "UDP";
}

//--------------------------------------------------------------------------------------------------
QString ConnectEnumerator::processName() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return QString();

        return processNameByPid(snapshot_.get(), tcp_table->table[pos_].dwOwningPid);
    }
    else
    {
        const MIB_UDPTABLE_OWNER_PID* udp_table =
            reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!udp_table)
            return QString();

        return processNameByPid(snapshot_.get(), udp_table->table[pos_].dwOwningPid);
    }
}

//--------------------------------------------------------------------------------------------------
QString ConnectEnumerator::localAddress() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return QString();

        return addressToString(tcp_table->table[pos_].dwLocalAddr);
    }
    else
    {
        const MIB_UDPTABLE_OWNER_PID* udp_table =
            reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!udp_table)
            return QString();

        return addressToString(udp_table->table[pos_].dwLocalAddr);
    }
}

//--------------------------------------------------------------------------------------------------
QString ConnectEnumerator::remoteAddress() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return QString();

        return addressToString(tcp_table->table[pos_].dwRemoteAddr);
    }
    else
    {
        return QString();
    }
}

//--------------------------------------------------------------------------------------------------
quint16 ConnectEnumerator::localPort() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return 0;

        return static_cast<quint16>(tcp_table->table[pos_].dwLocalPort);
    }
    else
    {
        const MIB_UDPTABLE_OWNER_PID* udp_table =
            reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!udp_table)
            return 0;

        return static_cast<quint16>(udp_table->table[pos_].dwLocalPort);
    }
}

//--------------------------------------------------------------------------------------------------
quint16 ConnectEnumerator::remotePort() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return 0;

        return static_cast<quint16>(tcp_table->table[pos_].dwRemotePort);
    }
    else
    {
        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
QString ConnectEnumerator::state() const
{
    if (mode_ == Mode::TCP)
    {
        const MIB_TCPTABLE_OWNER_PID* tcp_table =
            reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(table_buffer_.data());
        if (!tcp_table)
            return QString();

        return stateToString(tcp_table->table[pos_].dwState);
    }
    else
    {
        return QString();
    }
}

} // namespace base
