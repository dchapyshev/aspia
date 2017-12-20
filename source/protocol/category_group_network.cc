//
// PROJECT:         Aspia
// FILE:            protocol/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "network/network_adapter_enumerator.h"
#include "network/route_enumerator.h"
#include "protocol/category_group_network.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryOpenFiles
//

const char* CategoryOpenFiles::Name() const
{
    return "Open Files";
}

Category::IconId CategoryOpenFiles::Icon() const
{
    return IDI_FOLDER_NETWORK;
}

const char* CategoryOpenFiles::Guid() const
{
    return "{EAE638B9-CCF6-442C-84A1-B0901A64DA3D}";
}

void CategoryOpenFiles::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryOpenFiles::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryRoutes
//

const char* CategoryRoutes::Name() const
{
    return "Routes";
}

Category::IconId CategoryRoutes::Icon() const
{
    return IDI_ROUTE;
}

const char* CategoryRoutes::Guid() const
{
    return "{84184CEB-E232-4CA7-BCAC-E156F1E6DDCB}";
}

void CategoryRoutes::Parse(Table& table, const std::string& data)
{
    proto::Routes message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Destonation", 150)
                     .AddColumn("Mask", 150)
                     .AddColumn("Gateway", 150)
                     .AddColumn("Metric", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Routes::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.destonation()));
        row.AddValue(Value::String(item.mask()));
        row.AddValue(Value::String(item.gateway()));
        row.AddValue(Value::Number(item.metric()));
    }
}

std::string CategoryRoutes::Serialize()
{
    proto::Routes message;

    for (RouteEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Routes::Item* item = message.add_item();

        item->set_destonation(enumerator.GetDestonation());
        item->set_mask(enumerator.GetMask());
        item->set_gateway(enumerator.GetGateway());
        item->set_metric(enumerator.GetMetric());
    }

    return message.SerializeAsString();
}

//
// CategoryGroupNetwork
//

const char* CategoryGroupNetwork::Name() const
{
    return "Network";
}

Category::IconId CategoryGroupNetwork::Icon() const
{
    return IDI_NETWORK;
}

} // namespace aspia
