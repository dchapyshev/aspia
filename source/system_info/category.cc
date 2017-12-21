//
// PROJECT:         Aspia
// FILE:            system_info/category.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "protocol/category_group_software.h"

#include "system_info/category_application.h"
#include "system_info/category_ata.h"
#include "system_info/category_connection.h"
#include "system_info/category_cpu.h"
#include "system_info/category_dmi_baseboard.h"
#include "system_info/category_dmi_bios.h"
#include "system_info/category_dmi_cache.h"
#include "system_info/category_dmi_chassis.h"
#include "system_info/category_dmi_memory_device.h"
#include "system_info/category_dmi_onboard_device.h"
#include "system_info/category_dmi_pointing_device.h"
#include "system_info/category_dmi_port_connector.h"
#include "system_info/category_dmi_portable_battery.h"
#include "system_info/category_dmi_processor.h"
#include "system_info/category_dmi_system.h"
#include "system_info/category_dmi_system_slot.h"
#include "system_info/category_environment_variables.h"
#include "system_info/category_eventlog_application.h"
#include "system_info/category_eventlog_security.h"
#include "system_info/category_eventlog_system.h"
#include "system_info/category_license.h"
#include "system_info/category_memory.h"
#include "system_info/category_monitor.h"
#include "system_info/category_network_card.h"
#include "system_info/category_open_files.h"
#include "system_info/category_optical_drive.h"
#include "system_info/category_power_options.h"
#include "system_info/category_printer.h"
#include "system_info/category_process.h"
#include "system_info/category_ras.h"
#include "system_info/category_route.h"
#include "system_info/category_session.h"
#include "system_info/category_share.h"
#include "system_info/category_logical_drive.h"
#include "system_info/category_smart.h"
#include "system_info/category_task_scheduler.h"
#include "system_info/category_update.h"
#include "system_info/category_user.h"
#include "system_info/category_user_group.h"
#include "system_info/category_video_adapter.h"
#include "system_info/category_windows_device.h"
#include "ui/resource.h"

namespace aspia {

namespace {

class CategoryGroupDMI : public CategoryGroup
{
public:
    CategoryGroupDMI() = default;

    const char* Name() const final { return "DMI"; }
    IconId Icon() const final { return IDI_COMPUTER; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDMI);
};

class CategoryGroupStorage : public CategoryGroup
{
public:
    CategoryGroupStorage() = default;

    const char* Name() const final { return "Storage"; }
    IconId Icon() const final { return IDI_DRIVE; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupStorage);
};

class CategoryGroupDisplay : public CategoryGroup
{
public:
    CategoryGroupDisplay() = default;

    const char* Name() const final { return "Display"; }
    IconId Icon() const final { return IDI_MONITOR; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDisplay);
};

class CategoryGroupHardware : public CategoryGroup
{
public:
    CategoryGroupHardware() = default;

    const char* Name() const final { return "Hardware"; }
    IconId Icon() const final { return IDI_HARDWARE; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupHardware);
};

class CategoryGroupNetwork : public CategoryGroup
{
public:
    CategoryGroupNetwork() = default;

    const char* Name() const final { return "Network"; }
    IconId Icon() const final { return IDI_NETWORK; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupNetwork);
};

class CategoryGroupEventLog : public CategoryGroup
{
public:
    CategoryGroupEventLog() = default;

    const char* Name() const final { return "Event Logs"; }
    IconId Icon() const final { return IDI_BOOKS_STACK; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupEventLog);
};

class CategoryGroupUsers : public CategoryGroup
{
public:
    CategoryGroupUsers() = default;

    const char* Name() const final { return "Users and groups"; }
    IconId Icon() const final { return IDI_USERS; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupUsers);
};

class CategoryGroupOS : public CategoryGroup
{
public:
    CategoryGroupOS() = default;

    const char* Name() const final { return "Operating System"; }
    IconId Icon() const final { return IDI_OS; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupOS);
};

} // namespace

//
// Category
//

Category::Category(Type type) : type_(type)
{
    // Nothing
}

CategoryGroup* Category::category_group()
{
    DCHECK(type() == Type::GROUP);
    return reinterpret_cast<CategoryGroup*>(this);
}

CategoryInfo* Category::category_info()
{
    DCHECK(type() == Type::INFO_LIST || type() == Type::INFO_PARAM_VALUE);
    return reinterpret_cast<CategoryInfo*>(this);
}

//
// CategoryGroup
//

CategoryGroup::CategoryGroup() : Category(Type::GROUP)
{
    // Nothing
}

const CategoryList& CategoryGroup::child_list() const
{
    DCHECK(type() == Type::GROUP);
    return child_list_;
}

CategoryList* CategoryGroup::mutable_child_list()
{
    DCHECK(type() == Type::GROUP);
    return &child_list_;
}

//
// CategoryInfo
//

CategoryInfo::CategoryInfo(Type type) : Category(type)
{
    DCHECK(type == Type::INFO_LIST || type == Type::INFO_PARAM_VALUE);
}

//
// Functions
//

CategoryList CreateCategoryTree()
{
    std::unique_ptr<CategoryGroup> dmi = std::make_unique<CategoryGroupDMI>();

    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiBios>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiSystem>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiBaseboard>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiChassis>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiCache>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiProcessor>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiMemoryDevice>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiSystemSlot>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPortConnector>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiOnboardDevice>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPointingDevice>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPortableBattery>());

    std::unique_ptr<CategoryGroup> storage = std::make_unique<CategoryGroupStorage>();

    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryLogicalDrive>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryOpticalDrive>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryATA>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategorySMART>());

    std::unique_ptr<CategoryGroup> display = std::make_unique<CategoryGroupDisplay>();

    display->mutable_child_list()->emplace_back(std::make_unique<CategoryVideoAdapter>());
    display->mutable_child_list()->emplace_back(std::make_unique<CategoryMonitor>());

    std::unique_ptr<CategoryGroup> hardware = std::make_unique<CategoryGroupHardware>();

    hardware->mutable_child_list()->emplace_back(std::move(dmi));
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryCPU>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryMemory>());
    hardware->mutable_child_list()->emplace_back(std::move(storage));
    hardware->mutable_child_list()->emplace_back(std::move(display));
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryPowerOptions>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryPrinter>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryWindowsDevice>());

    std::unique_ptr<CategoryGroup> software = std::make_unique<CategoryGroupSoftware>();

    software->mutable_child_list()->emplace_back(std::make_unique<CategoryApplication>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryUpdate>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryServices>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryDrivers>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryProcess>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryLicense>());

    std::unique_ptr<CategoryGroup> network = std::make_unique<CategoryGroupNetwork>();

    network->mutable_child_list()->emplace_back(std::make_unique<CategoryNetworkCard>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryRAS>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryConnection>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryShare>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryOpenFiles>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryRoute>());

    std::unique_ptr<CategoryGroup> users_and_groups = std::make_unique<CategoryGroupUsers>();

    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategoryUser>());
    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategoryUserGroup>());
    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategorySession>());

    std::unique_ptr<CategoryGroup> event_logs = std::make_unique<CategoryGroupEventLog>();

    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogApplication>());
    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogSecurity>());
    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogSystem>());

    std::unique_ptr<CategoryGroup> os = std::make_unique<CategoryGroupOS>();

    os->mutable_child_list()->emplace_back(std::make_unique<CategoryTaskScheduler>());
    os->mutable_child_list()->emplace_back(std::move(users_and_groups));
    os->mutable_child_list()->emplace_back(std::make_unique<CategoryEnvironmentVariables>());
    os->mutable_child_list()->emplace_back(std::move(event_logs));

    CategoryList tree;

    tree.emplace_back(std::move(hardware));
    tree.emplace_back(std::move(software));
    tree.emplace_back(std::move(network));
    tree.emplace_back(std::move(os));

    return tree;
}

CategoryMap CreateCategoryMap()
{
    CategoryMap map;

    auto emplace_back = [&](std::unique_ptr<CategoryInfo> category)
    {
        DCHECK(category->Guid());
        std::string guid(category->Guid());
        map.emplace(std::move(guid), std::move(category));
    };

    emplace_back(std::make_unique<CategoryDmiBios>());
    emplace_back(std::make_unique<CategoryDmiSystem>());
    emplace_back(std::make_unique<CategoryDmiBaseboard>());
    emplace_back(std::make_unique<CategoryDmiChassis>());
    emplace_back(std::make_unique<CategoryDmiCache>());
    emplace_back(std::make_unique<CategoryDmiProcessor>());
    emplace_back(std::make_unique<CategoryDmiMemoryDevice>());
    emplace_back(std::make_unique<CategoryDmiSystemSlot>());
    emplace_back(std::make_unique<CategoryDmiPortConnector>());
    emplace_back(std::make_unique<CategoryDmiOnboardDevice>());
    emplace_back(std::make_unique<CategoryDmiPointingDevice>());
    emplace_back(std::make_unique<CategoryDmiPortableBattery>());

    emplace_back(std::make_unique<CategoryLogicalDrive>());
    emplace_back(std::make_unique<CategoryOpticalDrive>());
    emplace_back(std::make_unique<CategoryATA>());
    emplace_back(std::make_unique<CategorySMART>());

    emplace_back(std::make_unique<CategoryVideoAdapter>());
    emplace_back(std::make_unique<CategoryMonitor>());

    emplace_back(std::make_unique<CategoryWindowsDevice>());
    emplace_back(std::make_unique<CategoryCPU>());
    emplace_back(std::make_unique<CategoryMemory>());
    emplace_back(std::make_unique<CategoryPowerOptions>());
    emplace_back(std::make_unique<CategoryPrinter>());

    emplace_back(std::make_unique<CategoryApplication>());
    emplace_back(std::make_unique<CategoryUpdate>());
    emplace_back(std::make_unique<CategoryServices>());
    emplace_back(std::make_unique<CategoryDrivers>());
    emplace_back(std::make_unique<CategoryProcess>());
    emplace_back(std::make_unique<CategoryLicense>());

    emplace_back(std::make_unique<CategoryNetworkCard>());
    emplace_back(std::make_unique<CategoryRAS>());
    emplace_back(std::make_unique<CategoryConnection>());
    emplace_back(std::make_unique<CategoryShare>());
    emplace_back(std::make_unique<CategoryOpenFiles>());
    emplace_back(std::make_unique<CategoryRoute>());

    emplace_back(std::make_unique<CategoryUser>());
    emplace_back(std::make_unique<CategoryUserGroup>());
    emplace_back(std::make_unique<CategorySession>());

    emplace_back(std::make_unique<CategoryEventLogApplication>());
    emplace_back(std::make_unique<CategoryEventLogSecurity>());
    emplace_back(std::make_unique<CategoryEventLogSystem>());

    emplace_back(std::make_unique<CategoryTaskScheduler>());
    emplace_back(std::make_unique<CategoryEnvironmentVariables>());

    return map;
}

} // namespace aspia
