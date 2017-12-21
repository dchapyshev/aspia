//
// PROJECT:         Aspia
// FILE:            system_info/category_driver.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_enumerator.h"
#include "system_info/category_driver.h"
#include "system_info/category_driver.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* StatusToString(proto::Driver::Status status)
{
    switch (status)
    {
        case proto::Driver::STATUS_CONTINUE_PENDING:
            return "Continue Pending";

        case proto::Driver::STATUS_PAUSE_PENDING:
            return "Pause Pending";

        case proto::Driver::STATUS_PAUSED:
            return "Paused";

        case proto::Driver::STATUS_RUNNING:
            return "Running";

        case proto::Driver::STATUS_START_PENDING:
            return "Start Pending";

        case proto::Driver::STATUS_STOP_PENDING:
            return "Stop Pending";

        case proto::Driver::STATUS_STOPPED:
            return "Stopped";

        default:
            return "Unknown";
    }
}

const char* StartupTypeToString(proto::Driver::StartupType startup_type)
{
    switch (startup_type)
    {
        case proto::Driver::STARTUP_TYPE_AUTO_START:
            return "Auto Start";

        case proto::Driver::STARTUP_TYPE_DEMAND_START:
            return "Demand Start";

        case proto::Driver::STARTUP_TYPE_DISABLED:
            return "Disabled";

        case proto::Driver::STARTUP_TYPE_BOOT_START:
            return "Boot Start";

        case proto::Driver::STARTUP_TYPE_SYSTEM_START:
            return "System Start";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryDriver::CategoryDriver()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryDriver::Name() const
{
    return "Drivers";
}

Category::IconId CategoryDriver::Icon() const
{
    return IDI_PCI;
}

const char* CategoryDriver::Guid() const
{
    return "{8278DA10-227F-4484-9D5D-9A66C294CA82}";
}

void CategoryDriver::Parse(Table& table, const std::string& data)
{
    proto::Driver message;

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
        const proto::Driver::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.display_name()));
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(StatusToString(item.status())));
        row.AddValue(Value::String(StartupTypeToString(item.startup_type())));
        row.AddValue(Value::String(item.binary_path()));
    }
}

std::string CategoryDriver::Serialize()
{
    proto::Driver message;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
         !enumerator.IsAtEnd();
         enumerator.Advance())
    {
        proto::Driver::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_display_name(enumerator.GetDisplayName());
        item->set_description(enumerator.GetDescription());

        switch (enumerator.GetStatus())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(proto::Driver::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(proto::Driver::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                item->set_status(proto::Driver::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                item->set_status(proto::Driver::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                item->set_status(proto::Driver::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(proto::Driver::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                item->set_status(proto::Driver::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::Driver::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.GetStartupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::Driver::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(enumerator.GetBinaryPath());
    }

    return message.SerializeAsString();
}

} // namespace aspia
