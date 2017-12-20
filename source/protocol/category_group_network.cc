//
// PROJECT:         Aspia
// FILE:            protocol/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "network/network_adapter_enumerator.h"
#include "network/share_enumerator.h"
#include "network/route_enumerator.h"
#include "protocol/category_group_network.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategorySharedResources
//

const char* CategorySharedResources::Name() const
{
    return "Shared Resources";
}

Category::IconId CategorySharedResources::Icon() const
{
    return IDI_FOLDER_NETWORK;
}

const char* CategorySharedResources::Guid() const
{
    return "{9219D538-E1B8-453C-9298-61D5B80C4130}";
}

void CategorySharedResources::Parse(Table& table, const std::string& data)
{
    proto::SharedResources message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Name", 120)
                     .AddColumn("Type", 70)
                     .AddColumn("Description", 100)
                     .AddColumn("Local Path", 150)
                     .AddColumn("Current Uses", 100)
                     .AddColumn("Maximum Uses", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::SharedResources::Item& item = message.item(index);

        Row row = table.AddRow();

        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(TypeToString(item.type())));
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(item.local_path()));
        row.AddValue(Value::Number(item.current_uses()));
        row.AddValue(Value::Number(item.maximum_uses()));
    }
}

std::string CategorySharedResources::Serialize()
{
    proto::SharedResources message;

    for (ShareEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::SharedResources::Item* item = message.add_item();

        item->set_name(enumerator.GetName());

        switch (enumerator.GetType())
        {
            case ShareEnumerator::Type::DISK:
                item->set_type(proto::SharedResources::Item::TYPE_DISK);
                break;

            case ShareEnumerator::Type::PRINTER:
                item->set_type(proto::SharedResources::Item::TYPE_PRINTER);
                break;

            case ShareEnumerator::Type::DEVICE:
                item->set_type(proto::SharedResources::Item::TYPE_DEVICE);
                break;

            case ShareEnumerator::Type::IPC:
                item->set_type(proto::SharedResources::Item::TYPE_IPC);
                break;

            case ShareEnumerator::Type::SPECIAL:
                item->set_type(proto::SharedResources::Item::TYPE_SPECIAL);
                break;

            case ShareEnumerator::Type::TEMPORARY:
                item->set_type(proto::SharedResources::Item::TYPE_TEMPORARY);
                break;

            default:
                item->set_type(proto::SharedResources::Item::TYPE_UNKNOWN);
                break;
        }

        item->set_local_path(enumerator.GetLocalPath());
        item->set_current_uses(enumerator.GetCurrentUses());
        item->set_maximum_uses(enumerator.GetMaximumUses());
    }

    return message.SerializeAsString();
}

    // static
const char* CategorySharedResources::TypeToString(proto::SharedResources::Item::Type type)
{
    switch (type)
    {
        case proto::SharedResources::Item::TYPE_DISK:
            return "Disk";

        case proto::SharedResources::Item::TYPE_PRINTER:
            return "Printer";

        case proto::SharedResources::Item::TYPE_DEVICE:
            return "Device";

        case proto::SharedResources::Item::TYPE_IPC:
            return "IPC";

        case proto::SharedResources::Item::TYPE_SPECIAL:
            return "Special";

        case proto::SharedResources::Item::TYPE_TEMPORARY:
            return "Temporary";

        default:
            return "Unknown";
    }
}

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
