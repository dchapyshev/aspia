//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_software.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_enumerator.h"
#include "protocol/system_info_constants.h"
#include "protocol/category_group_software.h"
#include "protocol/category_info.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

class CategoryPrograms : public CategoryInfo
{
public:
    CategoryPrograms()
        : CategoryInfo(system_info::software::kPrograms, "Programs", IDI_APPLICATIONS)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrograms);
};

class CategoryUpdates : public CategoryInfo
{
public:
    CategoryUpdates()
        : CategoryInfo(system_info::software::kUpdates, "Updates", IDI_APPLICATIONS)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUpdates);
};

class CategoryServices : public CategoryInfo
{
public:
    CategoryServices() : CategoryInfo(system_info::software::kServices, "Services", IDI_GEAR)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Services message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name());

        {
            Output::TableHeader header(output);
            output->AddHeaderItem("Display Name", 200);
            output->AddHeaderItem("Name", 200);
            output->AddHeaderItem("Description", 200);
            output->AddHeaderItem("Status", 200);
            output->AddHeaderItem("Startup Type", 200);
            output->AddHeaderItem("Account", 200);
            output->AddHeaderItem("Executable File", 200);
        }

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::Services::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.display_name());
            output->AddValue(item.name());
            output->AddValue(item.description());
            output->AddValue(StatusToString(item.status()));
            output->AddValue(StartupTypeToString(item.startup_type()));
            output->AddValue(item.start_name());
            output->AddValue(item.binary_path());
        }
    }

    std::string Serialize() final
    {
        system_info::Services message;

        for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
             !enumerator.IsAtEnd();
             enumerator.Advance())
        {
            system_info::Services::Item* item = message.add_item();

            item->set_name(enumerator.GetName());
            item->set_display_name(enumerator.GetDisplayName());
            item->set_description(enumerator.GetDescription());

            switch (enumerator.GetStatus())
            {
                case ServiceEnumerator::Status::CONTINUE_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_CONTINUE_PENDING);
                    break;

                case ServiceEnumerator::Status::PAUSE_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_PAUSE_PENDING);
                    break;

                case ServiceEnumerator::Status::PAUSED:
                    item->set_status(system_info::Services::Item::STATUS_PAUSED);
                    break;

                case ServiceEnumerator::Status::RUNNING:
                    item->set_status(system_info::Services::Item::STATUS_RUNNING);
                    break;

                case ServiceEnumerator::Status::START_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_START_PENDING);
                    break;

                case ServiceEnumerator::Status::STOP_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_STOP_PENDING);
                    break;

                case ServiceEnumerator::Status::STOPPED:
                    item->set_status(system_info::Services::Item::STATUS_STOPPED);
                    break;

                default:
                    item->set_status(system_info::Services::Item::STATUS_UNKNOWN);
                    break;
            }

            switch (enumerator.GetStartupType())
            {
                case ServiceEnumerator::StartupType::AUTO_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_AUTO_START);
                    break;

                case ServiceEnumerator::StartupType::DEMAND_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DEMAND_START);
                    break;

                case ServiceEnumerator::StartupType::DISABLED:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DISABLED);
                    break;

                case ServiceEnumerator::StartupType::BOOT_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_BOOT_START);
                    break;

                case ServiceEnumerator::StartupType::SYSTEM_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_SYSTEM_START);
                    break;

                default:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_UNKNOWN);
                    break;
            }

            item->set_binary_path(enumerator.GetBinaryPath());
            item->set_start_name(enumerator.GetStartName());
        }

        return message.SerializeAsString();
    }

    static const char* StatusToString(system_info::Services::Item::Status status)
    {
        switch (status)
        {
            case system_info::Services::Item::STATUS_CONTINUE_PENDING:
                return "Continue Pending";

            case system_info::Services::Item::STATUS_PAUSE_PENDING:
                return "Pause Pending";

            case system_info::Services::Item::STATUS_PAUSED:
                return "Paused";

            case system_info::Services::Item::STATUS_RUNNING:
                return "Running";

            case system_info::Services::Item::STATUS_START_PENDING:
                return "Start Pending";

            case system_info::Services::Item::STATUS_STOP_PENDING:
                return "Stop Pending";

            case system_info::Services::Item::STATUS_STOPPED:
                return "Stopped";

            default:
                return "Unknown";
        }
    }

    static const char* StartupTypeToString(system_info::Services::Item::StartupType startup_type)
    {
        switch (startup_type)
        {
            case system_info::Services::Item::STARTUP_TYPE_AUTO_START:
                return "Auto Start";

            case system_info::Services::Item::STARTUP_TYPE_DEMAND_START:
                return "Demand Start";

            case system_info::Services::Item::STARTUP_TYPE_DISABLED:
                return "Disabled";

            case system_info::Services::Item::STARTUP_TYPE_BOOT_START:
                return "Boot Start";

            case system_info::Services::Item::STARTUP_TYPE_SYSTEM_START:
                return "System Start";

            default:
                return "Unknown";
        }
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryServices);
};

class CategoryDrivers : public CategoryInfo
{
public:
    CategoryDrivers() : CategoryInfo(system_info::software::kDrivers, "Drivers", IDI_PCI)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Services message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name());

        {
            Output::TableHeader header(output);
            output->AddHeaderItem("Display Name", 200);
            output->AddHeaderItem("Name", 200);
            output->AddHeaderItem("Description", 200);
            output->AddHeaderItem("Status", 200);
            output->AddHeaderItem("Startup Type", 200);
            output->AddHeaderItem("Executable File", 200);
        }

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::Services::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.display_name());
            output->AddValue(item.name());
            output->AddValue(item.description());
            output->AddValue(CategoryServices::StatusToString(item.status()));
            output->AddValue(CategoryServices::StartupTypeToString(item.startup_type()));
            output->AddValue(item.binary_path());
        }
    }

    std::string Serialize() final
    {
        system_info::Services message;

        for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
             !enumerator.IsAtEnd();
             enumerator.Advance())
        {
            system_info::Services::Item* item = message.add_item();

            item->set_name(enumerator.GetName());
            item->set_display_name(enumerator.GetDisplayName());
            item->set_description(enumerator.GetDescription());

            switch (enumerator.GetStatus())
            {
                case ServiceEnumerator::Status::CONTINUE_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_CONTINUE_PENDING);
                    break;

                case ServiceEnumerator::Status::PAUSE_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_PAUSE_PENDING);
                    break;

                case ServiceEnumerator::Status::PAUSED:
                    item->set_status(system_info::Services::Item::STATUS_PAUSED);
                    break;

                case ServiceEnumerator::Status::RUNNING:
                    item->set_status(system_info::Services::Item::STATUS_RUNNING);
                    break;

                case ServiceEnumerator::Status::START_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_START_PENDING);
                    break;

                case ServiceEnumerator::Status::STOP_PENDING:
                    item->set_status(system_info::Services::Item::STATUS_STOP_PENDING);
                    break;

                case ServiceEnumerator::Status::STOPPED:
                    item->set_status(system_info::Services::Item::STATUS_STOPPED);
                    break;

                default:
                    item->set_status(system_info::Services::Item::STATUS_UNKNOWN);
                    break;
            }

            switch (enumerator.GetStartupType())
            {
                case ServiceEnumerator::StartupType::AUTO_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_AUTO_START);
                    break;

                case ServiceEnumerator::StartupType::DEMAND_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DEMAND_START);
                    break;

                case ServiceEnumerator::StartupType::DISABLED:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_DISABLED);
                    break;

                case ServiceEnumerator::StartupType::BOOT_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_BOOT_START);
                    break;

                case ServiceEnumerator::StartupType::SYSTEM_START:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_SYSTEM_START);
                    break;

                default:
                    item->set_startup_type(system_info::Services::Item::STARTUP_TYPE_UNKNOWN);
                    break;
            }

            item->set_binary_path(enumerator.GetBinaryPath());
            item->set_start_name(enumerator.GetStartName());
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDrivers);
};

class CategoryProcesses : public CategoryInfo
{
public:
    CategoryProcesses()
        : CategoryInfo(system_info::software::kProcesses, "Processes", IDI_SYSTEM_MONITOR)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryProcesses);
};

class CategoryLicenses : public CategoryInfo
{
public:
    CategoryLicenses()
        : CategoryInfo(system_info::software::kLicenses, "Licenses", IDI_LICENSE_KEY)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryLicenses);
};

CategoryGroupSoftware::CategoryGroupSoftware()
    : CategoryGroup("Software", IDI_SOFTWARE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryPrograms>());
    child_list->emplace_back(std::make_unique<CategoryUpdates>());
    child_list->emplace_back(std::make_unique<CategoryServices>());
    child_list->emplace_back(std::make_unique<CategoryDrivers>());
    child_list->emplace_back(std::make_unique<CategoryProcesses>());
    child_list->emplace_back(std::make_unique<CategoryLicenses>());
}

} // namespace aspia
