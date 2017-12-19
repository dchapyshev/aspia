//
// PROJECT:         Aspia
// FILE:            system_info/category.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "protocol/category_group_software.h"
#include "protocol/category_group_network.h"
#include "protocol/category_group_os.h"

#include "system_info/category_ata.h"
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
#include "system_info/category_memory.h"
#include "system_info/category_monitor.h"
#include "system_info/category_optical_drive.h"
#include "system_info/category_power_options.h"
#include "system_info/category_printer.h"
#include "system_info/category_process.h"
#include "system_info/category_logical_drive.h"
#include "system_info/category_smart.h"
#include "system_info/category_video_adapter.h"
#include "system_info/category_windows_device.h"
#include "ui/resource.h"

namespace aspia {

namespace {

//
// CategoryGroupDMI
//

class CategoryGroupDMI : public CategoryGroup
{
public:
    CategoryGroupDMI() = default;

    const char* Name() const final { return "DMI"; }
    IconId Icon() const final { return IDI_COMPUTER; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDMI);
};

//
// CategoryGroupStorage
//

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

//
// CategoryGroupHardware
//

class CategoryGroupHardware : public CategoryGroup
{
public:
    CategoryGroupHardware() = default;

    const char* Name() const final { return "Hardware"; }
    IconId Icon() const final { return IDI_HARDWARE; }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupHardware);
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

    software->mutable_child_list()->emplace_back(std::make_unique<CategoryPrograms>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryUpdates>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryServices>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryDrivers>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryProcess>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryLicenses>());

    std::unique_ptr<CategoryGroup> network = std::make_unique<CategoryGroupNetwork>();

    network->mutable_child_list()->emplace_back(std::make_unique<CategoryNetworkCards>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryRasConnections>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryOpenConnections>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategorySharedResources>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryOpenFiles>());
    network->mutable_child_list()->emplace_back(std::make_unique<CategoryRoutes>());

    std::unique_ptr<CategoryGroup> users_and_groups = std::make_unique<CategoryGroupUsers>();

    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategoryUsers>());
    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategoryUserGroups>());
    users_and_groups->mutable_child_list()->emplace_back(std::make_unique<CategoryActiveSessions>());

    std::unique_ptr<CategoryGroup> event_logs = std::make_unique<CategoryGroupEventLog>();

    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogsApplications>());
    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogsSecurity>());
    event_logs->mutable_child_list()->emplace_back(std::make_unique<CategoryEventLogsSystem>());

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

    emplace_back(std::make_unique<CategoryPrograms>());
    emplace_back(std::make_unique<CategoryUpdates>());
    emplace_back(std::make_unique<CategoryServices>());
    emplace_back(std::make_unique<CategoryDrivers>());
    emplace_back(std::make_unique<CategoryProcess>());
    emplace_back(std::make_unique<CategoryLicenses>());

    emplace_back(std::make_unique<CategoryNetworkCards>());
    emplace_back(std::make_unique<CategoryRasConnections>());
    emplace_back(std::make_unique<CategoryOpenConnections>());
    emplace_back(std::make_unique<CategorySharedResources>());
    emplace_back(std::make_unique<CategoryOpenFiles>());
    emplace_back(std::make_unique<CategoryRoutes>());

    emplace_back(std::make_unique<CategoryUsers>());
    emplace_back(std::make_unique<CategoryUserGroups>());
    emplace_back(std::make_unique<CategoryActiveSessions>());

    emplace_back(std::make_unique<CategoryEventLogsApplications>());
    emplace_back(std::make_unique<CategoryEventLogsSecurity>());
    emplace_back(std::make_unique<CategoryEventLogsSystem>());

    emplace_back(std::make_unique<CategoryTaskScheduler>());
    emplace_back(std::make_unique<CategoryEnvironmentVariables>());

    return map;
}

} // namespace aspia
