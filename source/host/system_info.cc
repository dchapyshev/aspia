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

#include "base/environment.h"
#include "base/logging.h"
#include "base/smbios_parser.h"
#include "base/smbios_reader.h"
#include "base/sys_info.h"
#include "base/net/adapter_enumerator.h"
#include "base/net/connect_enumerator.h"
#include "base/net/route_enumerator.h"
#include "base/win/device_enumerator.h"
#include "base/win/drive_enumerator.h"
#include "base/win/printer_enumerator.h"
#include "base/win/monitor_enumerator.h"
#include "base/win/net_share_enumerator.h"
#include "base/win/service_enumerator.h"
#include "base/win/video_adapter_enumerator.h"
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

void fillMonitors(proto::SystemInfo* system_info)
{
    for (base::win::MonitorEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        std::unique_ptr<base::Edid> edid = enumerator.edid();
        if (!edid)
        {
            LOG(LS_INFO) << "No EDID information for monitor";
            continue;
        }

        proto::system_info::Monitors::Monitor* monitor =
            system_info->mutable_monitors()->add_monitor();

        std::string system_name = enumerator.friendlyName();
        if (system_name.empty())
            system_name = enumerator.description();

        monitor->set_system_name(system_name);
        monitor->set_monitor_name(edid->monitorName());
        monitor->set_manufacturer_name(edid->manufacturerName());
        monitor->set_monitor_id(edid->monitorId());
        monitor->set_serial_number(edid->serialNumber());
        monitor->set_edid_version(edid->edidVersion());
        monitor->set_edid_revision(edid->edidRevision());
        monitor->set_week_of_manufacture(edid->weekOfManufacture());
        monitor->set_year_of_manufacture(edid->yearOfManufacture());
        monitor->set_max_horizontal_image_size(edid->maxHorizontalImageSize());
        monitor->set_max_vertical_image_size(edid->maxVerticalImageSize());
        monitor->set_horizontal_resolution(edid->horizontalResolution());
        monitor->set_vertical_resoulution(edid->verticalResolution());
        monitor->set_gamma(edid->gamma());
        monitor->set_max_horizontal_rate(edid->maxHorizontalRate());
        monitor->set_min_horizontal_rate(edid->minHorizontalRate());
        monitor->set_max_vertical_rate(edid->maxVerticalRate());
        monitor->set_min_vertical_rate(edid->minVerticalRate());
        monitor->set_pixel_clock(edid->pixelClock());
        monitor->set_max_pixel_clock(edid->maxSupportedPixelClock());

        switch (edid->inputSignalType())
        {
            case base::Edid::INPUT_SIGNAL_TYPE_DIGITAL:
                monitor->set_input_signal_type(
                    proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_DIGITAL);
                break;

            case base::Edid::INPUT_SIGNAL_TYPE_ANALOG:
                monitor->set_input_signal_type(
                    proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_ANALOG);
                break;

            default:
                break;
        }

        uint8_t supported_features = edid->featureSupport();

        if (supported_features & base::Edid::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            monitor->set_default_gtf_supported(true);

        if (supported_features & base::Edid::FEATURE_SUPPORT_SUSPEND)
            monitor->set_suspend_supported(true);

        if (supported_features & base::Edid::FEATURE_SUPPORT_STANDBY)
            monitor->set_standby_supported(true);

        if (supported_features & base::Edid::FEATURE_SUPPORT_ACTIVE_OFF)
            monitor->set_active_off_supported(true);

        if (supported_features & base::Edid::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            monitor->set_preferred_timing_mode_supported(true);

        if (supported_features & base::Edid::FEATURE_SUPPORT_SRGB)
            monitor->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            proto::system_info::Monitors::Monitor::Timing* timing = monitor->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        uint8_t estabilished_timings1 = edid->estabilishedTimings1();

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_800X600_60HZ)
            add_timing(800, 600, 60);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_800X600_56HZ)
            add_timing(800, 600, 56);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_640X480_75HZ)
            add_timing(640, 480, 75);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_640X480_72HZ)
            add_timing(640, 480, 72);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_640X480_67HZ)
            add_timing(640, 480, 67);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_640X480_60HZ)
            add_timing(640, 480, 60);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_720X400_88HZ)
            add_timing(720, 400, 88);

        if (estabilished_timings1 & base::Edid::ESTABLISHED_TIMINGS_1_720X400_70HZ)
            add_timing(720, 400, 70);

        uint8_t estabilished_timings2 = edid->estabilishedTimings2();

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_1280X1024_75HZ)
            add_timing(1280, 1024, 75);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_1024X768_75HZ)
            add_timing(1024, 768, 75);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_1024X768_70HZ)
            add_timing(1024, 768, 70);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_1024X768_60HZ)
            add_timing(1024, 768, 60);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_1024X768_87HZ)
            add_timing(1024, 768, 87);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_832X624_75HZ)
            add_timing(832, 624, 75);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_800X600_75HZ)
            add_timing(800, 600, 75);

        if (estabilished_timings2 & base::Edid::ESTABLISHED_TIMINGS_2_800X600_72HZ)
            add_timing(800, 600, 72);

        uint8_t manufacturer_timings = edid->manufacturersTimings();

        if (manufacturer_timings & base::Edid::MANUFACTURERS_TIMINGS_1152X870_75HZ)
            add_timing(1152, 870, 75);

        for (int index = 0; index < edid->standardTimingsCount(); ++index)
        {
            int width, height, freq;

            if (edid->standardTimings(index, &width, &height, &freq))
                add_timing(width, height, freq);
        }
    }
}

void fillConnection(proto::SystemInfo* system_info)
{
    for (base::ConnectEnumerator enumerator(base::ConnectEnumerator::Mode::TCP);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Connections::Connection* connection =
            system_info->mutable_connections()->add_connection();

        connection->set_protocol(enumerator.protocol());
        connection->set_process_name(enumerator.processName());
        connection->set_local_address(enumerator.localAddress());
        connection->set_remote_address(enumerator.remoteAddress());
        connection->set_local_port(enumerator.localPort());
        connection->set_remote_port(enumerator.remotePort());
        connection->set_state(enumerator.state());
    }

    for (base::ConnectEnumerator enumerator(base::ConnectEnumerator::Mode::UDP);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Connections::Connection* connection =
            system_info->mutable_connections()->add_connection();

        connection->set_protocol(enumerator.protocol());
        connection->set_process_name(enumerator.processName());
        connection->set_local_address(enumerator.localAddress());
        connection->set_local_port(enumerator.localPort());
    }
}

void fillRoutes(proto::SystemInfo* system_info)
{
    for (base::RouteEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::Routes::Route* route = system_info->mutable_routes()->add_route();

        route->set_destonation(enumerator.destonation());
        route->set_mask(enumerator.mask());
        route->set_gateway(enumerator.gateway());
        route->set_metric(enumerator.metric());
    }
}

void fillEnvironmentVariables(proto::SystemInfo* system_info)
{
    std::vector<std::pair<std::string, std::string>> list = base::Environment::list();

    for (const auto& item : list)
    {
        proto::system_info::EnvironmentVariables::Variable* variable =
            system_info->mutable_env_vars()->add_variable();

        variable->set_name(item.first);
        variable->set_value(item.second);
    }
}

void fillVideoAdapters(proto::SystemInfo* system_info)
{
    for (base::win::VideoAdapterEnumarator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::VideoAdapters::Adapter* adapter =
            system_info->mutable_video_adapters()->add_adapter();

        adapter->set_description(enumerator.description());
        adapter->set_adapter_string(enumerator.adapterString());
        adapter->set_bios_string(enumerator.biosString());
        adapter->set_chip_type(enumerator.chipString());
        adapter->set_dac_type(enumerator.dacType());
        adapter->set_driver_date(enumerator.driverDate());
        adapter->set_driver_version(enumerator.driverVersion());
        adapter->set_driver_provider(enumerator.driverVendor());
        adapter->set_memory_size(enumerator.memorySize());
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
        fillVideoAdapters(system_info);
    }
    else if (category == common::kSystemInfo_Monitors)
    {
        fillMonitors(system_info);
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
        fillEnvironmentVariables(system_info);
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
        fillRoutes(system_info);
    }
    else if (category == common::kSystemInfo_Connections)
    {
        fillConnection(system_info);
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
