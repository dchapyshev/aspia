//
// PROJECT:         Aspia
// FILE:            protocol/category_group_software.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_enumerator.h"
#include "base/program_enumerator.h"
#include "base/service_enumerator.h"
#include "protocol/category_group_software.h"
#include "ui/system_info/output.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryServices
//

const char* CategoryServices::Name() const
{
    return "Services";
}

Category::IconId CategoryServices::Icon() const
{
    return IDI_GEAR;
}

const char* CategoryServices::Guid() const
{
    return "{BE3143AB-67C3-4EFE-97F5-FA0C84F338C3}";
}

void CategoryServices::Parse(Table& table, const std::string& data)
{
    proto::Services message;

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
        const proto::Services::Item& item = message.item(index);

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

std::string CategoryServices::Serialize()
{
    proto::Services message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::Services::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(proto::Services::Item::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(proto::Services::Item::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(proto::Services::Item::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(proto::Services::Item::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(proto::Services::Item::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(proto::Services::Item::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(proto::Services::Item::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::Services::Item::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
        item->set_start_name(enumerator.GetStartName());
    }

    return message.SerializeAsString();
}

// static
const char* CategoryServices::StatusToString(proto::Services::Item::Status status)
{
    switch (status)
    {
        case proto::Services::Item::STATUS_CONTINUE_PENDING:
            return "Continue Pending";

        case proto::Services::Item::STATUS_PAUSE_PENDING:
            return "Pause Pending";

        case proto::Services::Item::STATUS_PAUSED:
            return "Paused";

        case proto::Services::Item::STATUS_RUNNING:
            return "Running";

        case proto::Services::Item::STATUS_START_PENDING:
            return "Start Pending";

        case proto::Services::Item::STATUS_STOP_PENDING:
            return "Stop Pending";

        case proto::Services::Item::STATUS_STOPPED:
            return "Stopped";

        default:
            return "Unknown";
    }
}

// static
const char* CategoryServices::StartupTypeToString(
    proto::Services::Item::StartupType startup_type)
{
    switch (startup_type)
    {
        case proto::Services::Item::STARTUP_TYPE_AUTO_START:
            return "Auto Start";

        case proto::Services::Item::STARTUP_TYPE_DEMAND_START:
            return "Demand Start";

        case proto::Services::Item::STARTUP_TYPE_DISABLED:
            return "Disabled";

        case proto::Services::Item::STARTUP_TYPE_BOOT_START:
            return "Boot Start";

        case proto::Services::Item::STARTUP_TYPE_SYSTEM_START:
            return "System Start";

        default:
            return "Unknown";
    }
}

//
// CategoryDrivers
//

const char* CategoryDrivers::Name() const
{
    return "Drivers";
}

Category::IconId CategoryDrivers::Icon() const
{
    return IDI_PCI;
}

const char* CategoryDrivers::Guid() const
{
    return "{8278DA10-227F-4484-9D5D-9A66C294CA82}";
}

void CategoryDrivers::Parse(Table& table, const std::string& data)
{
    proto::Services message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Display Name", 200)
                     .AddColumn("Name", 150)
                     .AddColumn("Description", 200)
                     .AddColumn("Status", 70)
                     .AddColumn("Startup Type", 100)
                     .AddColumn("Executable File", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Services::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.display_name()));
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(CategoryServices::StatusToString(item.status())));
        row.AddValue(Value::String(CategoryServices::StartupTypeToString(item.startup_type())));
        row.AddValue(Value::String(item.binary_path()));
    }
}

std::string CategoryDrivers::Serialize()
{
    proto::Services message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::Services::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(proto::Services::Item::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(proto::Services::Item::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(proto::Services::Item::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(proto::Services::Item::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(proto::Services::Item::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(proto::Services::Item::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(proto::Services::Item::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::Services::Item::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::Services::Item::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
        item->set_start_name(enumerator.GetStartName());
    }

    return message.SerializeAsString();
}

//
// CategoryGroupSoftware
//

const char* CategoryGroupSoftware::Name() const
{
    return "Software";
}

Category::IconId CategoryGroupSoftware::Icon() const
{
    return IDI_SOFTWARE;
}

} // namespace aspia
