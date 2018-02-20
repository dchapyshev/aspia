//
// PROJECT:         Aspia
// FILE:            category/category_route.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_route.h"
#include "category/category_route.pb.h"
#include "ui/resource.h"

#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <memory>

namespace aspia {

namespace {

std::string IpToString(DWORD ip)
{
    char buffer[46 + 1];

    if (!inet_ntop(AF_INET, &ip, buffer, _countof(buffer)))
        return std::string();

    return buffer;
}

} // namespace

CategoryRoute::CategoryRoute()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryRoute::Name() const
{
    return "Routes";
}

Category::IconId CategoryRoute::Icon() const
{
    return IDI_ROUTE;
}

const char* CategoryRoute::Guid() const
{
    return "{84184CEB-E232-4CA7-BCAC-E156F1E6DDCB}";
}

void CategoryRoute::Parse(Table& table, const std::string& data)
{
    proto::Route message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Destonation", 150)
                     .AddColumn("Mask", 150)
                     .AddColumn("Gateway", 150)
                     .AddColumn("Metric", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Route::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.destonation()));
        row.AddValue(Value::String(item.mask()));
        row.AddValue(Value::String(item.gateway()));
        row.AddValue(Value::Number(item.metric()));
    }
}

std::string CategoryRoute::Serialize()
{
    ULONG buffer_size = sizeof(MIB_IPFORWARDTABLE);

    std::unique_ptr<uint8_t[]> forward_table_buffer =
        std::make_unique<uint8_t[]>(buffer_size);

    PMIB_IPFORWARDTABLE forward_table =
        reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer.get());

    DWORD error_code = GetIpForwardTable(forward_table, &buffer_size, 0);
    if (error_code != NO_ERROR)
    {
        if (error_code == ERROR_INSUFFICIENT_BUFFER)
        {
            forward_table_buffer = std::make_unique<uint8_t[]>(buffer_size);
            forward_table = reinterpret_cast<PMIB_IPFORWARDTABLE>(forward_table_buffer.get());

            error_code = GetIpForwardTable(forward_table, &buffer_size, 0);
        }
    }

    if (error_code != NO_ERROR)
        return std::string();

    proto::Route message;

    for (DWORD i = 0; i < forward_table->dwNumEntries; ++i)
    {
        proto::Route::Item* item = message.add_item();

        item->set_destonation(IpToString(forward_table->table[i].dwForwardDest));
        item->set_mask(IpToString(forward_table->table[i].dwForwardMask));
        item->set_gateway(IpToString(forward_table->table[i].dwForwardNextHop));
        item->set_metric(forward_table->table[i].dwForwardMetric1);
    }

    return message.SerializeAsString();
}

} // namespace aspia
