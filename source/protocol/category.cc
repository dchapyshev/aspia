//
// PROJECT:         Aspia
// FILE:            protocol/category.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "protocol/category_group_hardware.h"
#include "protocol/category_group_software.h"
#include "protocol/category_group_network.h"
#include "protocol/category_group_os.h"

#include "system_info/category_cpu.h"
#include "system_info/category_memory.h"
#include "system_info/category_logical_drive.h"

namespace aspia {

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
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiCaches>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiProcessors>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiMemoryDevices>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiSystemSlots>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPortConnectors>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiOnboardDevices>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPointingDevices>());
    dmi->mutable_child_list()->emplace_back(std::make_unique<CategoryDmiPortableBattery>());

    std::unique_ptr<CategoryGroup> storage = std::make_unique<CategoryGroupStorage>();

    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryLogicalDrives>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryOpticalDrives>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategoryATA>());
    storage->mutable_child_list()->emplace_back(std::make_unique<CategorySMART>());

    std::unique_ptr<CategoryGroup> display = std::make_unique<CategoryGroupDisplay>();

    display->mutable_child_list()->emplace_back(std::make_unique<CategoryVideoAdapters>());
    display->mutable_child_list()->emplace_back(std::make_unique<CategoryMonitor>());

    std::unique_ptr<CategoryGroup> hardware = std::make_unique<CategoryGroupHardware>();

    hardware->mutable_child_list()->emplace_back(std::move(dmi));
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryCPU>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryMemory>());
    hardware->mutable_child_list()->emplace_back(std::move(storage));
    hardware->mutable_child_list()->emplace_back(std::move(display));
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryPowerOptions>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryPrinters>());
    hardware->mutable_child_list()->emplace_back(std::make_unique<CategoryWindowsDevices>());

    std::unique_ptr<CategoryGroup> software = std::make_unique<CategoryGroupSoftware>();

    software->mutable_child_list()->emplace_back(std::make_unique<CategoryPrograms>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryUpdates>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryServices>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryDrivers>());
    software->mutable_child_list()->emplace_back(std::make_unique<CategoryProcesses>());
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
    emplace_back(std::make_unique<CategoryDmiCaches>());
    emplace_back(std::make_unique<CategoryDmiProcessors>());
    emplace_back(std::make_unique<CategoryDmiMemoryDevices>());
    emplace_back(std::make_unique<CategoryDmiSystemSlots>());
    emplace_back(std::make_unique<CategoryDmiPortConnectors>());
    emplace_back(std::make_unique<CategoryDmiOnboardDevices>());
    emplace_back(std::make_unique<CategoryDmiPointingDevices>());
    emplace_back(std::make_unique<CategoryDmiPortableBattery>());

    emplace_back(std::make_unique<CategoryLogicalDrives>());
    emplace_back(std::make_unique<CategoryOpticalDrives>());
    emplace_back(std::make_unique<CategoryATA>());
    emplace_back(std::make_unique<CategorySMART>());

    emplace_back(std::make_unique<CategoryVideoAdapters>());
    emplace_back(std::make_unique<CategoryMonitor>());

    emplace_back(std::make_unique<CategoryWindowsDevices>());
    emplace_back(std::make_unique<CategoryCPU>());
    emplace_back(std::make_unique<CategoryMemory>());
    emplace_back(std::make_unique<CategoryPowerOptions>());
    emplace_back(std::make_unique<CategoryPrinters>());

    emplace_back(std::make_unique<CategoryPrograms>());
    emplace_back(std::make_unique<CategoryUpdates>());
    emplace_back(std::make_unique<CategoryServices>());
    emplace_back(std::make_unique<CategoryDrivers>());
    emplace_back(std::make_unique<CategoryProcesses>());
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
