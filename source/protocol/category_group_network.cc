//
// PROJECT:         Aspia
// FILE:            protocol/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "network/network_adapter_enumerator.h"
#include "network/open_connection_enumerator.h"
#include "network/share_enumerator.h"
#include "network/route_enumerator.h"
#include "protocol/category_group_network.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryRasConnections
//

const char* CategoryRasConnections::Name() const
{
    return "RAS Connections";
}

Category::IconId CategoryRasConnections::Icon() const
{
    return IDI_TELEPHONE_FAX;
}

const char* CategoryRasConnections::Guid() const
{
    return "{E0A43CFD-3A97-4577-B3FB-3B542C0729F7}";
}

void CategoryRasConnections::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryRasConnections::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryOpenConnections
//

const char* CategoryOpenConnections::Name() const
{
    return "Open Connections";
}

Category::IconId CategoryOpenConnections::Icon() const
{
    return IDI_SERVERS_NETWORK;
}

const char* CategoryOpenConnections::Guid() const
{
    return "{1A9CBCBD-5623-4CEC-B58C-BD7BD8FAE622}";
}

void CategoryOpenConnections::Parse(Table& table, const std::string& data)
{
    proto::OpenConnections message;

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
        const proto::OpenConnections::Item& item = message.item(index);

        Row row = table.AddRow();

        row.AddValue(Value::String(item.process_name()));

        if (item.protocol() == proto::OpenConnections::PROTOCOL_TCP)
        {
            row.AddValue(Value::String("TCP"));
        }
        else if (item.protocol() == proto::OpenConnections::PROTOCOL_UDP)
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

std::string CategoryOpenConnections::Serialize()
{
    proto::OpenConnections message;

    for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::TCP);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::OpenConnections::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_protocol(proto::OpenConnections::PROTOCOL_TCP);
        item->set_local_address(enumerator.GetLocalAddress());
        item->set_remote_address(enumerator.GetRemoteAddress());
        item->set_local_port(enumerator.GetLocalPort());
        item->set_remote_port(enumerator.GetRemotePort());
        item->set_state(enumerator.GetState());
    }

    for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::UDP);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::OpenConnections::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_protocol(proto::OpenConnections::PROTOCOL_UDP);
        item->set_local_address(enumerator.GetLocalAddress());
        item->set_remote_address(enumerator.GetRemoteAddress());
        item->set_local_port(enumerator.GetLocalPort());
        item->set_remote_port(enumerator.GetRemotePort());
        item->set_state(enumerator.GetState());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryOpenConnections::StateToString(proto::OpenConnections::State value)
{
    switch (value)
    {
        case proto::OpenConnections::STATE_CLOSED:
            return "CLOSED";

        case proto::OpenConnections::STATE_LISTENING:
            return "LISTENING";

        case proto::OpenConnections::STATE_SYN_SENT:
            return "SYN_SENT";

        case proto::OpenConnections::STATE_SYN_RCVD:
            return "SYN_RCVD";

        case proto::OpenConnections::STATE_ESTABLISHED:
            return "ESTABLISHED";

        case proto::OpenConnections::STATE_FIN_WAIT1:
            return "FIN_WAIT1";

        case proto::OpenConnections::STATE_FIN_WAIT2:
            return "FIN_WAIT2";

        case proto::OpenConnections::STATE_CLOSE_WAIT:
            return "CLOSE_WAIT";

        case proto::OpenConnections::STATE_CLOSING:
            return "CLOSING";

        case proto::OpenConnections::STATE_LAST_ACK:
            return "LAST_ACK";

        case proto::OpenConnections::STATE_TIME_WAIT:
            return "TIME_WAIT";

        case proto::OpenConnections::STATE_DELETE_TCB:
            return "DELETE_TCB";

        default:
            return "Unknown";
    }
}

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
