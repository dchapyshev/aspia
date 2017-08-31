//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/open_connection_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/byte_order.h"
#include "base/logging.h"
#include "network/open_connection_enumerator.h"

#include <tlhelp32.h>
#include <winsock2.h>
#include <ws2def.h>

namespace aspia {

static bool GetProcessNameByPid(HANDLE process_snapshot,
                                DWORD process_id,
                                std::wstring& process_name)
{
    PROCESSENTRY32W process_entry;
    process_entry.dwSize = sizeof(process_entry);

    if (!Process32FirstW(process_snapshot, &process_entry))
        return false;

    do
    {
        if (process_entry.th32ProcessID == process_id)
        {
            process_name = process_entry.szExeFile;
            return true;
        }
    }
    while (Process32NextW(process_snapshot, &process_entry));

    return false;
}

static std::string AddressToString(uint32_t address)
{
    address = ByteSwap(address);

    return StringPrintf("%u.%u.%u.%u",
                        (address >> 24) & 0xFF,
                        (address >> 16) & 0xFF,
                        (address >> 8)  & 0xFF,
                        (address)       & 0xFF);
}

OpenConnectionEnumerator::OpenConnectionEnumerator(Type type)
{
    switch (type)
    {
        case Type::TCP:
            InitializeTcpTable();
            break;

        case Type::UDP:
            InitializeUdpTable();
            break;

        default:
            DLOG(FATAL) << "Unknown type: " << static_cast<int>(type);
            break;
    }

    process_snapshot_.Reset(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
}

void OpenConnectionEnumerator::InitializeTcpTable()
{
    ULONG table_buffer_size = sizeof(MIB_TCPTABLE);

    table_buffer_ = std::make_unique<uint8_t[]>(table_buffer_size);
    tcp_table_ = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer_.get());

    DWORD ret = GetExtendedTcpTable(tcp_table_, &table_buffer_size,
                                    TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer_ = std::make_unique<uint8_t[]>(table_buffer_size);
        tcp_table_ = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer_.get());

        ret = GetExtendedTcpTable(tcp_table_, &table_buffer_size,
                                  TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    }

    if (ret != NO_ERROR)
    {
        table_buffer_.reset();
        tcp_table_ = nullptr;
    }
}

void OpenConnectionEnumerator::InitializeUdpTable()
{
    ULONG table_buffer_size = sizeof(MIB_UDPTABLE);

    table_buffer_ = std::make_unique<uint8_t[]>(table_buffer_size);
    udp_table_ = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer_.get());

    DWORD ret = GetExtendedUdpTable(udp_table_, &table_buffer_size,
                                    TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer_ = std::make_unique<uint8_t[]>(table_buffer_size);
        udp_table_ = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer_.get());

        ret = GetExtendedUdpTable(udp_table_, &table_buffer_size,
                                  TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    }

    if (ret != NO_ERROR)
    {
        table_buffer_.reset();
        udp_table_ = nullptr;
    }
}

bool OpenConnectionEnumerator::IsAtEnd() const
{
    if (tcp_table_)
        return index_ >= tcp_table_->dwNumEntries;

    if (udp_table_)
        return index_ >= udp_table_->dwNumEntries;

    return true;
}

void OpenConnectionEnumerator::Advance()
{
    ++index_;
}

std::string OpenConnectionEnumerator::GetProcessName() const
{
    DWORD pid = -1;

    if (tcp_table_)
    {
        pid = tcp_table_->table[index_].dwOwningPid;
    }
    else if (udp_table_)
    {
        pid = udp_table_->table[index_].dwOwningPid;
    }

    if (pid != -1)
    {
        std::wstring process_name;

        if (GetProcessNameByPid(process_snapshot_.Get(), pid, process_name))
            return UTF8fromUNICODE(process_name);
    }

    return std::string();
}

std::string OpenConnectionEnumerator::GetLocalAddress() const
{
    if (tcp_table_)
    {
        return AddressToString(tcp_table_->table[index_].dwLocalAddr);
    }

    if (udp_table_)
    {
        return AddressToString(udp_table_->table[index_].dwLocalAddr);
    }

    return std::string();
}

std::string OpenConnectionEnumerator::GetRemoteAddress() const
{
    if (tcp_table_)
    {
        return AddressToString(tcp_table_->table[index_].dwRemoteAddr);
    }

    return std::string();
}

uint16_t OpenConnectionEnumerator::GetLocalPort() const
{
    if (tcp_table_)
    {
        return ByteSwap(static_cast<uint16_t>(tcp_table_->table[index_].dwLocalPort));
    }

    if (udp_table_)
    {
        return ByteSwap(static_cast<uint16_t>(udp_table_->table[index_].dwLocalPort));
    }

    return 0;
}

uint16_t OpenConnectionEnumerator::GetRemotePort() const
{
    if (tcp_table_)
    {
        return ByteSwap(static_cast<uint16_t>(tcp_table_->table[index_].dwRemotePort));
    }

    return 0;
}

std::string OpenConnectionEnumerator::GetState() const
{
    const char state_table[][32] =
    {
        "UNKNOWN",
        "CLOSED",
        "LISTENING",
        "SYN_SENT",
        "SYN_RCVD",
        "ESTABLISHED",
        "FIN_WAIT1",
        "FIN_WAIT2",
        "CLOSE_WAIT",
        "CLOSING",
        "LAST_ACK",
        "TIME_WAIT",
        "DELETE_TCB"
    };

    if (tcp_table_)
    {
        return state_table[tcp_table_->table[index_].dwState];
    }

    return std::string();
}

} // namespace aspia
