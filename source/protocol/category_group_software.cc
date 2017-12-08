//
// PROJECT:         Aspia Remote Desktop
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
// CategoryPrograms
//

const char* CategoryPrograms::Name() const
{
    return "Programs";
}

Category::IconId CategoryPrograms::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryPrograms::Guid() const
{
    return "606C70BE-0C6C-4CB6-90E6-D374760FC5EE";
}

void CategoryPrograms::Parse(Output* output, const std::string& data)
{
    proto::Programs message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, this, Output::TableType::LIST);

    output->Add(ColumnList::Create()
                .AddColumn("Name", 200)
                .AddColumn("Version", 100)
                .AddColumn("Publisher", 100)
                .AddColumn("Install Date", 80)
                .AddColumn("Install Location", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Programs::Item& item = message.item(index);

        Output::Row row(output);

        output->AddValue(Value::String(item.name()));
        output->AddValue(Value::String(item.version()));
        output->AddValue(Value::String(item.publisher()));
        output->AddValue(Value::String(item.install_date()));
        output->AddValue(Value::String(item.install_location()));
    }
}

std::string CategoryPrograms::Serialize()
{
    proto::Programs message;

    for (ProgramEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Programs::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_version(enumerator.GetVersion());
        item->set_publisher(enumerator.GetPublisher());
        item->set_install_date(enumerator.GetInstallDate());
        item->set_install_location(enumerator.GetInstallLocation());
    }

    return message.SerializeAsString();
}

//
// CategoryUpdates
//

const char* CategoryUpdates::Name() const
{
    return "Updates";
}

Category::IconId CategoryUpdates::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryUpdates::Guid() const
{
    return "3E160E27-BE2E-45DB-8292-C3786C9533AB";
}

void CategoryUpdates::Parse(Output* /* output */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryUpdates::Serialize()
{
    // TODO
    return std::string();
}

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
    return "BE3143AB-67C3-4EFE-97F5-FA0C84F338C3";
}

void CategoryServices::Parse(Output* output, const std::string& data)
{
    proto::Services message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, this, Output::TableType::LIST);

    output->Add(ColumnList::Create()
                .AddColumn("Display Name", 200)
                .AddColumn("Name", 200)
                .AddColumn("Description", 200)
                .AddColumn("Status", 200)
                .AddColumn("Startup Type", 200)
                .AddColumn("Account", 200)
                .AddColumn("Executable File", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Services::Item& item = message.item(index);

        Output::Row row(output);

        output->AddValue(Value::String(item.display_name()));
        output->AddValue(Value::String(item.name()));
        output->AddValue(Value::String(item.description()));
        output->AddValue(Value::String(StatusToString(item.status())));
        output->AddValue(Value::String(StartupTypeToString(item.startup_type())));
        output->AddValue(Value::String(item.start_name()));
        output->AddValue(Value::String(item.binary_path()));
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
    return "8278DA10-227F-4484-9D5D-9A66C294CA82";
}

void CategoryDrivers::Parse(Output* output, const std::string& data)
{
    proto::Services message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, this, Output::TableType::LIST);

    output->Add(ColumnList::Create()
                .AddColumn("Display Name", 200)
                .AddColumn("Name", 200)
                .AddColumn("Description", 200)
                .AddColumn("Status", 200)
                .AddColumn("Startup Type", 200)
                .AddColumn("Executable File", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Services::Item& item = message.item(index);

        Output::Row row(output);

        output->AddValue(Value::String(item.display_name()));
        output->AddValue(Value::String(item.name()));
        output->AddValue(Value::String(item.description()));
        output->AddValue(Value::String(CategoryServices::StatusToString(item.status())));
        output->AddValue(Value::String(CategoryServices::StartupTypeToString(item.startup_type())));
        output->AddValue(Value::String(item.binary_path()));
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
// CategoryProcesses
//

const char* CategoryProcesses::Name() const
{
    return "Processes";
}

Category::IconId CategoryProcesses::Icon() const
{
    return IDI_SYSTEM_MONITOR;
}

const char* CategoryProcesses::Guid() const
{
    return "14BB101B-EE61-49E6-B5B9-874C4DBEA03C";
}

void CategoryProcesses::Parse(Output* output, const std::string& data)
{
    proto::Processes message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, this, Output::TableType::LIST);

    output->Add(ColumnList::Create()
                .AddColumn("Process Name", 150)
                .AddColumn("File Path", 200)
                .AddColumn("Used Memory", 80)
                .AddColumn("Used Swap", 80)
                .AddColumn("Description", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Processes::Item& item = message.item(index);

        Output::Row row(output);

        output->AddValue(Value::String(item.process_name()));
        output->AddValue(Value::String(item.file_path()));

        if (item.used_memory() != 0)
        {
            output->AddValue(Value::Number(item.used_memory() / 1024, "kB"));
        }
        else
        {
            output->AddValue(Value::Empty());
        }

        if (item.used_swap() != 0)
        {
            output->AddValue(Value::Number(item.used_swap() / 1024, "kB"));
        }
        else
        {
            output->AddValue(Value::Empty());
        }

        output->AddValue(Value::String(item.description()));
    }
}

std::string CategoryProcesses::Serialize()
{
    proto::Processes message;

    for (ProcessEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Processes::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_file_path(enumerator.GetFilePath());
        item->set_used_memory(enumerator.GetUsedMemory());
        item->set_used_swap(enumerator.GetUsedSwap());
        item->set_description(enumerator.GetFileDescription());
    }

    return message.SerializeAsString();
}

//
// CategoryLicenses
//

const char* CategoryLicenses::Name() const
{
    return "Licenses";
}

Category::IconId CategoryLicenses::Icon() const
{
    return IDI_LICENSE_KEY;
}

const char* CategoryLicenses::Guid() const
{
    return "6BD88575-9D23-44BC-8A49-64D94CC3EE48";
}

void CategoryLicenses::Parse(Output* /* output */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryLicenses::Serialize()
{
    // TODO
    return std::string();
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
