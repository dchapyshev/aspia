//
// PROJECT:         Aspia
// FILE:            system_info/category_service.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_enumerator.h"
#include "system_info/category_service.h"
#include "system_info/category_service.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* StatusToString(proto::Service::Status status)
{
    switch (status)
    {
        case proto::Service::STATUS_CONTINUE_PENDING:
            return "Continue Pending";

        case proto::Service::STATUS_PAUSE_PENDING:
            return "Pause Pending";

        case proto::Service::STATUS_PAUSED:
            return "Paused";

        case proto::Service::STATUS_RUNNING:
            return "Running";

        case proto::Service::STATUS_START_PENDING:
            return "Start Pending";

        case proto::Service::STATUS_STOP_PENDING:
            return "Stop Pending";

        case proto::Service::STATUS_STOPPED:
            return "Stopped";

        default:
            return "Unknown";
    }
}

const char* StartupTypeToString(proto::Service::StartupType startup_type)
{
    switch (startup_type)
    {
        case proto::Service::STARTUP_TYPE_AUTO_START:
            return "Auto Start";

        case proto::Service::STARTUP_TYPE_DEMAND_START:
            return "Demand Start";

        case proto::Service::STARTUP_TYPE_DISABLED:
            return "Disabled";

        case proto::Service::STARTUP_TYPE_BOOT_START:
            return "Boot Start";

        case proto::Service::STARTUP_TYPE_SYSTEM_START:
            return "System Start";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryService::CategoryService()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryService::Name() const
{
    return "Services";
}

Category::IconId CategoryService::Icon() const
{
    return IDI_GEAR;
}

const char* CategoryService::Guid() const
{
    return "{BE3143AB-67C3-4EFE-97F5-FA0C84F338C3}";
}

void CategoryService::Parse(Table& table, const std::string& data)
{
    proto::Service message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Display Name", 200)
                     .AddColumn("Name", 150)
                     .AddColumn("Description", 200)
                     .AddColumn("Status", 70)
                     .AddColumn("Startup Type", 100)
                     .AddColumn("Account", 150)
                     .AddColumn("Executable File", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Service::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.display_name()));
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(StatusToString(item.status())));
        row.AddValue(Value::String(StartupTypeToString(item.startup_type())));
        row.AddValue(Value::String(item.start_name()));
        row.AddValue(Value::String(item.binary_path()));
    }
}

std::string CategoryService::Serialize()
{
    proto::Service message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::Service::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(proto::Service::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(proto::Service::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(proto::Service::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(proto::Service::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(proto::Service::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(proto::Service::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(proto::Service::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::Service::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(proto::Service::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(proto::Service::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(proto::Service::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(proto::Service::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(proto::Service::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::Service::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
        item->set_start_name(enumerator.GetStartName());
    }

    return message.SerializeAsString();
}

} // namespace aspia
