//
// PROJECT:         Aspia
// FILE:            category/category_connection.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/byte_order.h"
#include "base/scoped_object.h"
#include "category/category_connection.h"
#include "category/category_connection.pb.h"
#include "ui/resource.h"

#include <iphlpapi.h>
#include <tlhelp32.h>
#include <winsock2.h>
#include <ws2def.h>
#include <memory>

namespace aspia {

namespace {

std::unique_ptr<uint8_t[]> InitializeTcpTable()
{
    ULONG table_buffer_size = sizeof(MIB_TCPTABLE);

    std::unique_ptr<uint8_t[]> table_buffer = std::make_unique<uint8_t[]>(table_buffer_size);

    DWORD ret = GetExtendedTcpTable(reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer.get()),
                                    &table_buffer_size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer = std::make_unique<uint8_t[]>(table_buffer_size);

        ret = GetExtendedTcpTable(reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer.get()),
                                  &table_buffer_size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    }

    if (ret != NO_ERROR)
        return nullptr;

    return table_buffer;
}

std::unique_ptr<uint8_t[]> InitializeUdpTable()
{
    ULONG table_buffer_size = sizeof(MIB_UDPTABLE);

    std::unique_ptr<uint8_t[]> table_buffer = std::make_unique<uint8_t[]>(table_buffer_size);

    DWORD ret = GetExtendedUdpTable(reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer.get()),
                                    &table_buffer_size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);

    if (ret == ERROR_INSUFFICIENT_BUFFER)
    {
        table_buffer = std::make_unique<uint8_t[]>(table_buffer_size);

        ret = GetExtendedUdpTable(reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer.get()),
                                  &table_buffer_size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    }

    if (ret != NO_ERROR)
        return nullptr;

    return table_buffer;
}

std::string GetProcessNameByPid(HANDLE process_snapshot, DWORD process_id)
{
    PROCESSENTRY32W process_entry;
    process_entry.dwSize = sizeof(process_entry);

    if (Process32FirstW(process_snapshot, &process_entry))
    {
        do
        {
            if (process_entry.th32ProcessID == process_id)
                return UTF8fromUNICODE(process_entry.szExeFile);
        } while (Process32NextW(process_snapshot, &process_entry));
    }

    return std::string();
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

const char* StateToString(proto::Connection::State value)
{
    switch (value)
    {
        case proto::Connection::STATE_CLOSED:
            return "CLOSED";

        case proto::Connection::STATE_LISTENING:
            return "LISTENING";

        case proto::Connection::STATE_SYN_SENT:
            return "SYN_SENT";

        case proto::Connection::STATE_SYN_RCVD:
            return "SYN_RCVD";

        case proto::Connection::STATE_ESTABLISHED:
            return "ESTABLISHED";

        case proto::Connection::STATE_FIN_WAIT1:
            return "FIN_WAIT1";

        case proto::Connection::STATE_FIN_WAIT2:
            return "FIN_WAIT2";

        case proto::Connection::STATE_CLOSE_WAIT:
            return "CLOSE_WAIT";

        case proto::Connection::STATE_CLOSING:
            return "CLOSING";

        case proto::Connection::STATE_LAST_ACK:
            return "LAST_ACK";

        case proto::Connection::STATE_TIME_WAIT:
            return "TIME_WAIT";

        case proto::Connection::STATE_DELETE_TCB:
            return "DELETE_TCB";

        default:
            return "Unknown";
    }
}

proto::Connection::State ToState(DWORD state)
{
    switch (state)
    {
        case 0: return proto::Connection::STATE_UNKNOWN;
        case 1: return proto::Connection::STATE_CLOSED;
        case 2: return proto::Connection::STATE_LISTENING;
        case 3: return proto::Connection::STATE_SYN_SENT;
        case 4: return proto::Connection::STATE_SYN_RCVD;
        case 5: return proto::Connection::STATE_ESTABLISHED;
        case 6: return proto::Connection::STATE_FIN_WAIT1;
        case 7: return proto::Connection::STATE_FIN_WAIT2;
        case 8: return proto::Connection::STATE_CLOSE_WAIT;
        case 9: return proto::Connection::STATE_CLOSING;
        case 10: return proto::Connection::STATE_LAST_ACK;
        case 11: return proto::Connection::STATE_TIME_WAIT;
        case 12: return proto::Connection::STATE_DELETE_TCB;
        default: return proto::Connection::STATE_UNKNOWN;
    }
}

} // namespace

CategoryConnection::CategoryConnection()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryConnection::Name() const
{
    return "Open Connections";
}

Category::IconId CategoryConnection::Icon() const
{
    return IDI_SERVERS_NETWORK;
}

const char* CategoryConnection::Guid() const
{
    return "{1A9CBCBD-5623-4CEC-B58C-BD7BD8FAE622}";
}

void CategoryConnection::Parse(Table& table, const std::string& data)
{
    proto::Connection message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Process Name", 150)
                     .AddColumn("Protocol", 60)
                     .AddColumn("Local Address", 90)
                     .AddColumn("Local Port", 80)
                     .AddColumn("Remote Address", 90)
                     .AddColumn("Remote Port", 80)
                     .AddColumn("State", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Connection::Item& item = message.item(index);

        Row row = table.AddRow();

        row.AddValue(Value::String(item.process_name()));

        if (item.protocol() == proto::Connection::PROTOCOL_TCP)
        {
            row.AddValue(Value::String("TCP"));
        }
        else if (item.protocol() == proto::Connection::PROTOCOL_UDP)
        {
            row.AddValue(Value::String("UDP"));
        }
        else
        {
            row.AddValue(Value::String("Unknown"));
        }

        row.AddValue(Value::String(item.local_address()));
        row.AddValue(Value::Number(item.local_port()));
        row.AddValue(Value::String(item.remote_address()));
        row.AddValue(Value::Number(item.remote_port()));
        row.AddValue(Value::String(StateToString(item.state())));
    }
}

std::string CategoryConnection::Serialize()
{
    proto::Connection message;

    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

    std::unique_ptr<uint8_t[]> table_buffer = InitializeTcpTable();
    if (table_buffer)
    {
        PMIB_TCPTABLE_OWNER_PID tcp_table =
            reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(table_buffer.get());

        for (DWORD i = 0; i < tcp_table->dwNumEntries; ++i)
        {
            proto::Connection::Item* item = message.add_item();

            item->set_protocol(proto::Connection::PROTOCOL_TCP);
            item->set_process_name(GetProcessNameByPid(snapshot, tcp_table->table[i].dwOwningPid));
            item->set_local_address(AddressToString(tcp_table->table[i].dwLocalAddr));
            item->set_remote_address(AddressToString(tcp_table->table[i].dwRemoteAddr));
            item->set_local_port(ByteSwap(static_cast<uint16_t>(tcp_table->table[i].dwLocalPort)));
            item->set_remote_port(ByteSwap(static_cast<uint16_t>(tcp_table->table[i].dwRemotePort)));
            item->set_state(ToState(tcp_table->table[i].dwState));
        }
    }

    table_buffer = InitializeUdpTable();
    if (table_buffer)
    {
        PMIB_UDPTABLE_OWNER_PID udp_table =
            reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(table_buffer.get());

        for (DWORD i = 0; i < udp_table->dwNumEntries; ++i)
        {
            proto::Connection::Item* item = message.add_item();

            item->set_protocol(proto::Connection::PROTOCOL_UDP);
            item->set_process_name(GetProcessNameByPid(snapshot, udp_table->table[i].dwOwningPid));
            item->set_local_address(AddressToString(udp_table->table[i].dwLocalAddr));
            item->set_local_port(ByteSwap(static_cast<uint16_t>(udp_table->table[i].dwLocalPort)));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
