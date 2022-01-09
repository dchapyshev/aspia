//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/system_info.h"

#include "base/logging.h"
#include "base/smbios_parser.h"
#include "base/smbios_reader.h"
#include "base/sys_info.h"
#include "base/net/adapter_enumerator.h"
#include "base/win/device_enumerator.h"
#include "base/win/drive_enumerator.h"
#include "base/win/printer_enumerator.h"
#include "base/win/net_share_enumerator.h"
#include "base/win/service_enumerator.h"
#include "common/desktop_session_constants.h"

namespace host {

namespace {

void fillDevices(proto::SystemInfo* system_info)
{
    for (base::win::DeviceEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::WindowsDevices::Device* device =
            system_info->mutable_windows_devices()->add_device();

        device->set_friendly_name(enumerator.friendlyName());
        device->set_description(enumerator.description());
        device->set_driver_version(enumerator.driverVersion());
        device->set_driver_date(enumerator.driverDate());
        device->set_driver_vendor(enumerator.driverVendor());
        device->set_device_id(enumerator.deviceID());
    }
}

void fillPrinters(proto::SystemInfo* system_info)
{
    for (base::win::PrinterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::Printers::Printer* printer =
            system_info->mutable_printers()->add_printer();

        printer->set_name(enumerator.name());
        printer->set_default_(enumerator.isDefault());
        printer->set_shared(enumerator.isShared());
        printer->set_port(enumerator.portName());
        printer->set_driver(enumerator.driverName());
        printer->set_jobs_count(static_cast<uint32_t>(enumerator.jobsCount()));
        printer->set_share_name(enumerator.shareName());
    }
}

void fillNetworkAdapters(proto::SystemInfo* system_info)
{
    for (base::AdapterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::NetworkAdapters::Adapter* adapter =
            system_info->mutable_network_adapters()->add_adapter();

        adapter->set_adapter_name(enumerator.adapterName());
        adapter->set_connection_name(enumerator.connectionName());
        adapter->set_iface(enumerator.interfaceType());
        adapter->set_speed(enumerator.speed());
        adapter->set_mac(enumerator.macAddress());
        adapter->set_dhcp_enabled(enumerator.isDhcp4Enabled());

        if (enumerator.isDhcp4Enabled())
        {
            std::string dhcp4_server = enumerator.dhcp4Server();
            if (!dhcp4_server.empty())
                adapter->add_dhcp()->append(dhcp4_server);
        }

        for (base::AdapterEnumerator::GatewayEnumerator gateway(enumerator);
             !gateway.isAtEnd(); gateway.advance())
        {
            adapter->add_gateway()->assign(gateway.address());
        }

        for (base::AdapterEnumerator::IpAddressEnumerator ip(enumerator);
             !ip.isAtEnd(); ip.advance())
        {
            proto::system_info::NetworkAdapters::Adapter::Address* address = adapter->add_address();

            address->set_ip(ip.address());
            address->set_mask(ip.mask());
        }

        for (base::AdapterEnumerator::DnsEnumerator dns(enumerator);
             !dns.isAtEnd(); dns.advance())
        {
            adapter->add_dns()->assign(dns.address());
        }
    }
}

void fillNetworkShares(proto::SystemInfo* system_info)
{
    for (base::win::NetShareEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::NetworkShares::Share* share =
            system_info->mutable_network_shares()->add_share();

        share->set_name(enumerator.name());
        share->set_description(enumerator.description());
        share->set_local_path(enumerator.localPath());
        share->set_current_uses(enumerator.currentUses());
        share->set_max_uses(enumerator.maxUses());

        using ShareType = base::win::NetShareEnumerator::Type;

        switch (enumerator.type())
        {
            case ShareType::DISK:
                share->set_type("Disk");
                break;

            case ShareType::PRINTER:
                share->set_type("Printer");
                break;

            case ShareType::DEVICE:
                share->set_type("Device");
                break;

            case ShareType::IPC:
                share->set_type("IPC");
                break;

            case ShareType::SPECIAL:
                share->set_type("Special");
                break;

            case ShareType::TEMPORARY:
                share->set_type("Temporary");
                break;

            default:
                break;
        }
    }
}

void fillServices(proto::SystemInfo* system_info)
{
    using ServiceEnumerator = base::win::ServiceEnumerator;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Services::Service* service =
            system_info->mutable_services()->add_service();

        service->set_name(enumerator.name());
        service->set_display_name(enumerator.displayName());
        service->set_description(enumerator.description());

        switch (enumerator.status())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                service->set_status(proto::system_info::Services::Service::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                service->set_status(proto::system_info::Services::Service::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                service->set_status(proto::system_info::Services::Service::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                service->set_status(proto::system_info::Services::Service::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                service->set_status(proto::system_info::Services::Service::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                service->set_status(proto::system_info::Services::Service::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                service->set_status(proto::system_info::Services::Service::STATUS_STOPPED);
                break;

            default:
                service->set_status(proto::system_info::Services::Service::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.startupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                service->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_UNKNOWN);
                break;
        }

        service->set_binary_path(enumerator.binaryPath());
        service->set_start_name(enumerator.startName());
    }
}

void fillDrivers(proto::SystemInfo* system_info)
{
    using ServiceEnumerator = base::win::ServiceEnumerator;

    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Drivers::Driver* driver = system_info->mutable_drivers()->add_driver();

        driver->set_name(enumerator.name());
        driver->set_display_name(enumerator.displayName());
        driver->set_description(enumerator.description());

        switch (enumerator.status())
        {
            case ServiceEnumerator::Status::CONTINUE_PENDING:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_CONTINUE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSE_PENDING:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_PAUSE_PENDING);
                break;

            case ServiceEnumerator::Status::PAUSED:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_PAUSED);
                break;

            case ServiceEnumerator::Status::RUNNING:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_RUNNING);
                break;

            case ServiceEnumerator::Status::START_PENDING:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_START_PENDING);
                break;

            case ServiceEnumerator::Status::STOP_PENDING:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_STOP_PENDING);
                break;

            case ServiceEnumerator::Status::STOPPED:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_STOPPED);
                break;

            default:
                driver->set_status(proto::system_info::Drivers::Driver::STATUS_UNKNOWN);
                break;
        }

        switch (enumerator.startupType())
        {
            case ServiceEnumerator::StartupType::AUTO_START:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_AUTO_START);
                break;

            case ServiceEnumerator::StartupType::DEMAND_START:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_DEMAND_START);
                break;

            case ServiceEnumerator::StartupType::DISABLED:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_DISABLED);
                break;

            case ServiceEnumerator::StartupType::BOOT_START:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_BOOT_START);
                break;

            case ServiceEnumerator::StartupType::SYSTEM_START:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                driver->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_UNKNOWN);
                break;
        }

        driver->set_binary_path(enumerator.binaryPath());
    }
}

void createSummaryInfo(proto::SystemInfo* system_info)
{
    proto::system_info::Computer* computer = system_info->mutable_computer();
    computer->set_name(base::SysInfo::computerName());
    computer->set_domain(base::SysInfo::computerDomain());
    computer->set_workgroup(base::SysInfo::computerWorkgroup());
    computer->set_uptime(base::SysInfo::uptime());

    proto::system_info::OperatingSystem* operating_system = system_info->mutable_operating_system();
    operating_system->set_name(base::SysInfo::operatingSystemName());
    operating_system->set_version(base::SysInfo::operatingSystemVersion());
    operating_system->set_arch(base::SysInfo::operatingSystemArchitecture());
    operating_system->set_key(base::SysInfo::operatingSystemKey());
    operating_system->set_install_date(base::SysInfo::operatingSystemInstallDate());

    proto::system_info::Processor* processor = system_info->mutable_processor();
    processor->set_vendor(base::SysInfo::processorVendor());
    processor->set_model(base::SysInfo::processorName());
    processor->set_packages(static_cast<uint32_t>(base::SysInfo::processorPackages()));
    processor->set_cores(static_cast<uint32_t>(base::SysInfo::processorCores()));
    processor->set_threads(static_cast<uint32_t>(base::SysInfo::processorThreads()));

    for (base::SmbiosTableEnumerator enumerator(base::readSmbiosDump());
         !enumerator.isAtEnd(); enumerator.advance())
    {
        const base::SmbiosTable* table = enumerator.table();

        switch (table->type)
        {
            case base::SMBIOS_TABLE_TYPE_BIOS:
            {
                base::SmbiosBios bios_table(table);

                proto::system_info::Bios* bios = system_info->mutable_bios();
                bios->set_vendor(bios_table.vendor());
                bios->set_version(bios_table.version());
                bios->set_date(bios_table.releaseDate());
            }
            break;

            case base::SMBIOS_TABLE_TYPE_BASEBOARD:
            {
                base::SmbiosBaseboard baseboard_table(table);
                if (!baseboard_table.isValid())
                    continue;

                proto::system_info::Motherboard* motherboard = system_info->mutable_motherboard();
                motherboard->set_manufacturer(baseboard_table.manufacturer());
                motherboard->set_model(baseboard_table.product());
            }
            break;

            case base::SMBIOS_TABLE_TYPE_MEMORY_DEVICE:
            {
                base::SmbiosMemoryDevice memory_device_table(table);
                if (!memory_device_table.isValid())
                    continue;

                proto::system_info::Memory::Module* module =
                    system_info->mutable_memory()->add_module();

                module->set_present(memory_device_table.isPresent());
                module->set_location(memory_device_table.location());

                if (memory_device_table.isPresent())
                {
                    module->set_manufacturer(memory_device_table.manufacturer());
                    module->set_size(memory_device_table.size());
                    module->set_type(memory_device_table.type());
                    module->set_form_factor(memory_device_table.formFactor());
                    module->set_part_number(memory_device_table.partNumber());
                    module->set_speed(memory_device_table.speed());
                }
            }
            break;

            default:
                break;
        }
    }

    for (base::win::DriveEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        const base::win::DriveEnumerator::DriveInfo& drive_info = enumerator.driveInfo();

        proto::system_info::LogicalDrives::Drive* drive =
            system_info->mutable_logical_drives()->add_drive();

        drive->set_path(drive_info.path().u8string());
        drive->set_file_system(drive_info.fileSystem());
        drive->set_total_size(drive_info.totalSpace());
        drive->set_free_size(drive_info.freeSpace());
    }
}

} // namespace

void createSystemInfo(const std::string& serialized_request, proto::SystemInfo* system_info)
{
    if (serialized_request.empty())
    {
        createSummaryInfo(system_info);
        return;
    }

    proto::SystemInfoRequest request;
    if (!request.ParseFromString(serialized_request))
    {
        LOG(LS_WARNING) << "Unable to parse system info request";
        return;
    }

    const std::string& category = request.category();
    if (category.empty())
    {
        LOG(LS_WARNING) << "Unable to get system info category";
        return;
    }

    LOG(LS_INFO) << "Requested system info category: " << category;

    if (category == common::kSystemInfo_Devices)
    {
        fillDevices(system_info);
    }
    else if (category == common::kSystemInfo_VideoAdapters)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_Monitors)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_Printers)
    {
        fillPrinters(system_info);
    }
    else if (category == common::kSystemInfo_PowerOptions)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_Drivers)
    {
        fillDrivers(system_info);
    }
    else if (category == common::kSystemInfo_Services)
    {
        fillServices(system_info);
    }
    else if (category == common::kSystemInfo_EnvironmentVariables)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_EventLogs)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_NetworkAdapters)
    {
        fillNetworkAdapters(system_info);
    }
    else if (category == common::kSystemInfo_Routes)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_Connections)
    {
        // TODO
    }
    else if (category == common::kSystemInfo_NetworkShares)
    {
        fillNetworkShares(system_info);
    }
    else if (category == common::kSystemInfo_Ras)
    {
        // TODO
    }
    else
    {
        LOG(LS_WARNING) << "Unknown system info category: " << category;
    }
}

} // namespace host
