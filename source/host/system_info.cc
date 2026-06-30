//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QProcessEnvironment>
#include <QStorageInfo>

#include <thread>

#include "base/applications_reader.h"
#include "base/license_reader.h"
#include "base/logging.h"
#include "base/smbios_parser.h"
#include "base/sys_info.h"
#include "base/net/net_utils.h"
#include "common/system_info_constants.h"
#include "proto/system_info.h"

#if defined(Q_OS_WINDOWS)
#include "base/net/adapter_enumerator.h"
#include "base/net/connect_enumerator.h"
#include "base/net/open_files_enumerator.h"
#include "base/win/battery_enumerator.h"
#include "base/win/device_enumerator.h"
#include "base/win/event_enumerator.h"
#include "base/win/power_info.h"
#include "base/win/printer_enumerator.h"
#include "base/win/net_share_enumerator.h"
#include "base/win/service_enumerator.h"
#include "host/win/process_monitor.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillDevices(proto::system_info::SystemInfo* system_info)
{
    for (DeviceEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::WindowsDevices::Device* device =
            system_info->mutable_windows_devices()->add_device();

        device->set_friendly_name(enumerator.friendlyName().toStdString());
        device->set_description(enumerator.description().toStdString());
        device->set_driver_version(enumerator.driverVersion().toStdString());
        device->set_driver_date(enumerator.driverDate().toStdString());
        device->set_driver_vendor(enumerator.driverVendor().toStdString());
        device->set_device_id(enumerator.deviceID().toStdString());
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillPrinters(proto::system_info::SystemInfo* system_info)
{
    for (PrinterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::Printers::Printer* printer =
            system_info->mutable_printers()->add_printer();

        printer->set_name(enumerator.name().toStdString());
        printer->set_is_default(enumerator.isDefault());
        printer->set_is_shared(enumerator.isShared());
        printer->set_port(enumerator.portName().toStdString());
        printer->set_driver(enumerator.driverName().toStdString());
        printer->set_jobs_count(static_cast<quint32>(enumerator.jobsCount()));
        printer->set_share_name(enumerator.shareName().toStdString());
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillNetworkAdapters(proto::system_info::SystemInfo* system_info)
{
    for (AdapterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::NetworkAdapters::Adapter* adapter =
            system_info->mutable_network_adapters()->add_adapter();

        adapter->set_adapter_name(enumerator.adapterName().toStdString());
        adapter->set_connection_name(enumerator.connectionName().toStdString());
        adapter->set_iface(enumerator.interfaceType().toStdString());
        adapter->set_speed(enumerator.speed());
        adapter->set_mac(enumerator.macAddress().toStdString());
        adapter->set_dhcp_enabled(enumerator.isDhcp4Enabled());

        if (enumerator.isDhcp4Enabled())
        {
            QString dhcp4_server = enumerator.dhcp4Server();
            if (!dhcp4_server.isEmpty())
                adapter->add_dhcp()->append(dhcp4_server.toStdString());
        }

        for (AdapterEnumerator::GatewayEnumerator gateway(enumerator);
             !gateway.isAtEnd(); gateway.advance())
        {
            adapter->add_gateway()->assign(gateway.address().toStdString());
        }

        for (AdapterEnumerator::IpAddressEnumerator ip(enumerator);
             !ip.isAtEnd(); ip.advance())
        {
            proto::system_info::NetworkAdapters::Adapter::Address* address = adapter->add_address();

            address->set_ip(ip.address().toStdString());
            address->set_mask(ip.mask().toStdString());
        }

        for (AdapterEnumerator::DnsEnumerator dns(enumerator);
             !dns.isAtEnd(); dns.advance())
        {
            adapter->add_dns()->assign(dns.address().toStdString());
        }
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillNetworkShares(proto::system_info::SystemInfo* system_info)
{
    for (NetShareEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::NetworkShares::Share* share =
            system_info->mutable_network_shares()->add_share();

        share->set_name(enumerator.name().toStdString());
        share->set_description(enumerator.description().toStdString());
        share->set_local_path(enumerator.localPath().toStdString());
        share->set_current_uses(enumerator.currentUses());
        share->set_max_uses(enumerator.maxUses());

        using ShareType = NetShareEnumerator::Type;

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
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillServices(proto::system_info::SystemInfo* system_info)
{
    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::SERVICES);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Services::Service* service =
            system_info->mutable_services()->add_service();

        service->set_name(enumerator.name().toStdString());
        service->set_display_name(enumerator.displayName().toStdString());
        service->set_description(enumerator.description().toStdString());

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

        service->set_binary_path(enumerator.binaryPath().toStdString());
        service->set_start_name(enumerator.startName().toStdString());
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillDrivers(proto::system_info::SystemInfo* system_info)
{
    for (ServiceEnumerator enumerator(ServiceEnumerator::Type::DRIVERS);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Drivers::Driver* driver = system_info->mutable_drivers()->add_driver();

        driver->set_name(enumerator.name().toStdString());
        driver->set_display_name(enumerator.displayName().toStdString());
        driver->set_description(enumerator.description().toStdString());

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

        driver->set_binary_path(enumerator.binaryPath().toStdString());
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillMonitors(proto::system_info::SystemInfo* system_info)
{
    for (MonitorEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        Edid edid = enumerator.edid();
        if (!edid.isValid())
        {
            LOG(INFO) << "No EDID information for monitor";
            continue;
        }

        proto::system_info::Monitors::Monitor* monitor =
            system_info->mutable_monitors()->add_monitor();

        QString system_name = enumerator.friendlyName();
        if (system_name.isEmpty())
            system_name = enumerator.description();

        monitor->set_system_name(system_name.toStdString());
        monitor->set_monitor_name(edid.monitorName().toStdString());
        monitor->set_manufacturer_name(edid.manufacturerName().toStdString());
        monitor->set_monitor_id(edid.monitorId().toStdString());
        monitor->set_serial_number(edid.serialNumber().toStdString());
        monitor->set_edid_version(edid.edidVersion());
        monitor->set_edid_revision(edid.edidRevision());
        monitor->set_week_of_manufacture(edid.weekOfManufacture());
        monitor->set_year_of_manufacture(edid.yearOfManufacture());
        monitor->set_max_horizontal_image_size(edid.maxHorizontalImageSize());
        monitor->set_max_vertical_image_size(edid.maxVerticalImageSize());
        monitor->set_horizontal_resolution(edid.horizontalResolution());
        monitor->set_vertical_resoulution(edid.verticalResolution());
        monitor->set_gamma(edid.gamma());
        monitor->set_max_horizontal_rate(edid.maxHorizontalRate());
        monitor->set_min_horizontal_rate(edid.minHorizontalRate());
        monitor->set_max_vertical_rate(edid.maxVerticalRate());
        monitor->set_min_vertical_rate(edid.minVerticalRate());
        monitor->set_pixel_clock(edid.pixelClock());
        monitor->set_max_pixel_clock(edid.maxSupportedPixelClock());

        switch (edid.inputSignalType())
        {
            case Edid::INPUT_SIGNAL_TYPE_DIGITAL:
                monitor->set_input_signal_type(
                    proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_DIGITAL);
                break;

            case Edid::INPUT_SIGNAL_TYPE_ANALOG:
                monitor->set_input_signal_type(
                    proto::system_info::Monitors::Monitor::INPUT_SIGNAL_TYPE_ANALOG);
                break;

            default:
                break;
        }

        quint8 supported_features = edid.featureSupport();

        if (supported_features & Edid::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            monitor->set_default_gtf_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SUSPEND)
            monitor->set_suspend_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_STANDBY)
            monitor->set_standby_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_ACTIVE_OFF)
            monitor->set_active_off_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            monitor->set_preferred_timing_mode_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SRGB)
            monitor->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            proto::system_info::Monitors::Monitor::Timing* timing = monitor->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        quint8 estabilished_timings1 = edid.estabilishedTimings1();

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_60HZ)
            add_timing(800, 600, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_800X600_56HZ)
            add_timing(800, 600, 56);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_75HZ)
            add_timing(640, 480, 75);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_72HZ)
            add_timing(640, 480, 72);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_67HZ)
            add_timing(640, 480, 67);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_640X480_60HZ)
            add_timing(640, 480, 60);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_88HZ)
            add_timing(720, 400, 88);

        if (estabilished_timings1 & Edid::ESTABLISHED_TIMINGS_1_720X400_70HZ)
            add_timing(720, 400, 70);

        quint8 estabilished_timings2 = edid.estabilishedTimings2();

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1280X1024_75HZ)
            add_timing(1280, 1024, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_75HZ)
            add_timing(1024, 768, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_70HZ)
            add_timing(1024, 768, 70);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_60HZ)
            add_timing(1024, 768, 60);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_1024X768_87HZ)
            add_timing(1024, 768, 87);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_832X624_75HZ)
            add_timing(832, 624, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_75HZ)
            add_timing(800, 600, 75);

        if (estabilished_timings2 & Edid::ESTABLISHED_TIMINGS_2_800X600_72HZ)
            add_timing(800, 600, 72);

        quint8 manufacturer_timings = edid.manufacturersTimings();

        if (manufacturer_timings & Edid::MANUFACTURERS_TIMINGS_1152X870_75HZ)
            add_timing(1152, 870, 75);

        for (int index = 0; index < edid.standardTimingsCount(); ++index)
        {
            int width, height, freq;

            if (edid.standardTimings(index, &width, &height, &freq))
                add_timing(width, height, freq);
        }
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillConnection(proto::system_info::SystemInfo* system_info)
{
    for (ConnectEnumerator enumerator(ConnectEnumerator::Mode::TCP);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Connections::Connection* connection =
            system_info->mutable_connections()->add_connection();

        connection->set_protocol(enumerator.protocol().toStdString());
        connection->set_process_name(enumerator.processName().toStdString());
        connection->set_local_address(enumerator.localAddress().toStdString());
        connection->set_remote_address(enumerator.remoteAddress().toStdString());
        connection->set_local_port(enumerator.localPort());
        connection->set_remote_port(enumerator.remotePort());
        connection->set_state(enumerator.state().toStdString());
    }

    for (ConnectEnumerator enumerator(ConnectEnumerator::Mode::UDP);
         !enumerator.isAtEnd();
         enumerator.advance())
    {
        proto::system_info::Connections::Connection* connection =
            system_info->mutable_connections()->add_connection();

        connection->set_protocol(enumerator.protocol().toStdString());
        connection->set_process_name(enumerator.processName().toStdString());
        connection->set_local_address(enumerator.localAddress().toStdString());
        connection->set_local_port(enumerator.localPort());
    }
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void fillRoutes(proto::system_info::SystemInfo* system_info)
{
    const QList<NetUtils::Route> route_table = NetUtils::routeTable();
    for (const NetUtils::Route& entry : route_table)
    {
        proto::system_info::Routes::Route* route = system_info->mutable_routes()->add_route();

        route->set_destonation(entry.destination.toStdString());
        route->set_mask(entry.mask.toStdString());
        route->set_gateway(entry.gateway.toStdString());
        route->set_metric(entry.metric);
    }
}

//--------------------------------------------------------------------------------------------------
void fillEnvironmentVariables(proto::system_info::SystemInfo* system_info)
{
    QStringList list = QProcessEnvironment::systemEnvironment().toStringList();

    for (const auto& item : std::as_const(list))
    {
        proto::system_info::EnvironmentVariables::Variable* variable =
            system_info->mutable_env_vars()->add_variable();
        QStringList splitted = item.split('=');
        if (splitted.size() == 2)
        {
            variable->set_name(splitted[0].toStdString());
            variable->set_value(splitted[1].toStdString());
        }
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillVideoAdapters(proto::system_info::SystemInfo* system_info)
{
    for (VideoAdapterEnumarator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::VideoAdapters::Adapter* adapter =
            system_info->mutable_video_adapters()->add_adapter();

        adapter->set_description(enumerator.description().toStdString());
        adapter->set_adapter_string(enumerator.adapterString().toStdString());
        adapter->set_bios_string(enumerator.biosString().toStdString());
        adapter->set_chip_type(enumerator.chipString().toStdString());
        adapter->set_dac_type(enumerator.dacType().toStdString());
        adapter->set_driver_date(enumerator.driverDate().toStdString());
        adapter->set_driver_version(enumerator.driverVersion().toStdString());
        adapter->set_driver_provider(enumerator.driverVendor().toStdString());
        adapter->set_memory_size(enumerator.memorySize());
    }
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillPowerOptions(proto::system_info::SystemInfo* system_info)
{
    proto::system_info::PowerOptions* power_options = system_info->mutable_power_options();
    PowerInfo power_info;

    switch (power_info.powerSource())
    {
        case PowerInfo::PowerSource::DC_BATTERY:
            power_options->set_power_source(proto::system_info::PowerOptions::POWER_SOURCE_DC_BATTERY);
            break;

        case PowerInfo::PowerSource::AC_LINE:
            power_options->set_power_source(proto::system_info::PowerOptions::POWER_SOURCE_AC_LINE);
            break;

        default:
            break;
    }

    switch (power_info.batteryStatus())
    {
        case PowerInfo::BatteryStatus::HIGH:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_HIGH);
            break;

        case PowerInfo::BatteryStatus::LOW:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_LOW);
            break;

        case PowerInfo::BatteryStatus::CRITICAL:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_CRITICAL);
            break;

        case PowerInfo::BatteryStatus::CHARGING:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_CHARGING);
            break;

        case PowerInfo::BatteryStatus::NO_BATTERY:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_NO_BATTERY);
            break;

        default:
            break;
    }

    power_options->set_battery_life_percent(power_info.batteryLifePercent());
    power_options->set_full_battery_life_time(power_info.batteryFullLifeTime());
    power_options->set_remaining_battery_life_time(power_info.batteryRemainingLifeTime());

    for (BatteryEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::PowerOptions::Battery* battery = power_options->add_battery();
        battery->set_device_name(enumerator.deviceName().toStdString());
        battery->set_manufacturer(enumerator.manufacturer().toStdString());
        battery->set_manufacture_date(enumerator.manufactureDate().toStdString());
        battery->set_unique_id(enumerator.uniqueId().toStdString());
        battery->set_serial_number(enumerator.serialNumber().toStdString());
        battery->set_temperature(enumerator.temperature().toStdString());
        battery->set_design_capacity(enumerator.designCapacity());
        battery->set_type(enumerator.type().toStdString());
        battery->set_full_charged_capacity(enumerator.fullChargedCapacity());
        battery->set_depreciation(enumerator.depreciation());
        battery->set_current_capacity(enumerator.currentCapacity());
        battery->set_voltage(enumerator.voltage());

        auto append_state = [&](proto::system_info::PowerOptions::Battery::State state)
        {
            battery->set_state(battery->state() | static_cast<quint32>(state));
        };

        quint32 state = enumerator.state();

        if (state & BatteryEnumerator::CHARGING)
            append_state(proto::system_info::PowerOptions::Battery::STATE_CHARGING);

        if (state & BatteryEnumerator::CRITICAL)
            append_state(proto::system_info::PowerOptions::Battery::STATE_CRITICAL);

        if (state & BatteryEnumerator::DISCHARGING)
            append_state(proto::system_info::PowerOptions::Battery::STATE_DISCHARGING);

        if (state & BatteryEnumerator::POWER_ONLINE)
            append_state(proto::system_info::PowerOptions::Battery::STATE_POWER_ONLINE);
    }
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void fillComputer(proto::system_info::SystemInfo* system_info)
{
    proto::system_info::Computer* computer = system_info->mutable_computer();
    computer->set_name(SysInfo::computerName().toStdString());
    computer->set_domain(SysInfo::computerDomain().toStdString());
    computer->set_workgroup(SysInfo::computerWorkgroup().toStdString());
    computer->set_uptime(SysInfo::uptime());
}

//--------------------------------------------------------------------------------------------------
void fillOperatingSystem(proto::system_info::SystemInfo* system_info)
{
    proto::system_info::OperatingSystem* operating_system = system_info->mutable_operating_system();
    operating_system->set_name(SysInfo::operatingSystemName().toStdString());
    operating_system->set_version(SysInfo::operatingSystemVersion().toStdString());
    operating_system->set_arch(SysInfo::operatingSystemArchitecture().toStdString());
#if defined(Q_OS_WINDOWS)
    operating_system->set_key(SysInfo::operatingSystemKey().toStdString());
    operating_system->set_install_date(SysInfo::operatingSystemInstallDate());
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void fillProcessor(proto::system_info::SystemInfo* system_info)
{
    proto::system_info::Processor* processor = system_info->mutable_processor();
    processor->set_vendor(SysInfo::processorVendor().toStdString());
    processor->set_model(SysInfo::processorName().toStdString());
    processor->set_packages(static_cast<quint32>(SysInfo::processorPackages()));
    processor->set_cores(static_cast<quint32>(SysInfo::processorCores()));
    processor->set_threads(static_cast<quint32>(SysInfo::processorThreads()));
}

//--------------------------------------------------------------------------------------------------
void fillBios(proto::system_info::SystemInfo* system_info)
{
    for (SmbiosTableEnumerator enumerator(SysInfo::smbiosDump());
         !enumerator.isAtEnd(); enumerator.advance())
    {
        const SmbiosTable* table = enumerator.table();

        switch (table->type)
        {
            case SMBIOS_TABLE_TYPE_BIOS:
            {
                SmbiosBios bios_table(table);

                proto::system_info::Bios* bios = system_info->mutable_bios();
                bios->set_vendor(bios_table.vendor().toStdString());
                bios->set_version(bios_table.version().toStdString());
                bios->set_date(bios_table.releaseDate().toStdString());
            }
            break;

            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void fillMotherboard(proto::system_info::SystemInfo* system_info)
{
    for (SmbiosTableEnumerator enumerator(SysInfo::smbiosDump());
         !enumerator.isAtEnd(); enumerator.advance())
    {
        const SmbiosTable* table = enumerator.table();

        switch (table->type)
        {
            case SMBIOS_TABLE_TYPE_BASEBOARD:
            {
                SmbiosBaseboard baseboard_table(table);
                if (!baseboard_table.isValid())
                    continue;

                proto::system_info::Motherboard* motherboard = system_info->mutable_motherboard();
                motherboard->set_manufacturer(baseboard_table.manufacturer().toStdString());
                motherboard->set_model(baseboard_table.product().toStdString());
            }
            break;

            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void fillMemory(proto::system_info::SystemInfo* system_info)
{
    for (SmbiosTableEnumerator enumerator(SysInfo::smbiosDump());
         !enumerator.isAtEnd(); enumerator.advance())
    {
        const SmbiosTable* table = enumerator.table();

        switch (table->type)
        {
            case SMBIOS_TABLE_TYPE_MEMORY_DEVICE:
            {
                SmbiosMemoryDevice memory_device_table(table);
                if (!memory_device_table.isValid())
                    continue;

                proto::system_info::Memory::Module* module =
                    system_info->mutable_memory()->add_module();

                module->set_present(memory_device_table.isPresent());
                module->set_location(memory_device_table.location().toStdString());

                if (memory_device_table.isPresent())
                {
                    module->set_manufacturer(memory_device_table.manufacturer().toStdString());
                    module->set_size(memory_device_table.size());
                    module->set_type(memory_device_table.type().toStdString());
                    module->set_form_factor(memory_device_table.formFactor().toStdString());
                    module->set_part_number(memory_device_table.partNumber().toStdString());
                    module->set_speed(memory_device_table.speed());
                }
            }
            break;

            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void fillDrives(proto::system_info::SystemInfo* system_info)
{
    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    for (const auto& volume : std::as_const(volumes))
    {
        proto::system_info::LogicalDrives::Drive* drive =
            system_info->mutable_logical_drives()->add_drive();

        drive->set_path(volume.rootPath().toStdString());
        drive->set_file_system(volume.fileSystemType().toStdString());
        drive->set_total_size(volume.bytesTotal());
        drive->set_free_size(volume.bytesFree());
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillEventLogs(proto::system_info::SystemInfo* system_info,
                   const proto::system_info::EventLogsData& data)
{
    const char* log_name;
    switch (data.type())
    {
        case proto::system_info::EventLogs::Event::TYPE_APPLICATION:
            log_name = "Application";
            break;

        case proto::system_info::EventLogs::Event::TYPE_SECURITY:
            log_name = "Security";
            break;

        case proto::system_info::EventLogs::Event::TYPE_SYSTEM:
            log_name = "System";
            break;

        default:
            return;
    }

    EventEnumerator enumerator(log_name, data.record_start(), data.record_count());

    system_info->mutable_event_logs()->set_type(data.type());
    system_info->mutable_event_logs()->set_total_records(enumerator.count());

    while (!enumerator.isAtEnd())
    {
        proto::system_info::EventLogs::Event::Level level;
        switch (enumerator.type())
        {
            case EventEnumerator::Type::ERR:
                level = proto::system_info::EventLogs::Event::LEVEL_ERROR;
                break;

            case EventEnumerator::Type::WARN:
                level = proto::system_info::EventLogs::Event::LEVEL_WARNING;
                break;

            case EventEnumerator::Type::INFO:
                level = proto::system_info::EventLogs::Event::LEVEL_INFORMATION;
                break;

            case EventEnumerator::Type::AUDIT_SUCCESS:
                level = proto::system_info::EventLogs::Event::LEVEL_AUDIT_SUCCESS;
                break;

            case EventEnumerator::Type::AUDIT_FAILURE:
                level = proto::system_info::EventLogs::Event::LEVEL_AUDIT_FAILURE;
                break;

            case EventEnumerator::Type::SUCCESS:
                level = proto::system_info::EventLogs::Event::LEVEL_SUCCESS;
                break;

            default:
                continue;
        }

        proto::system_info::EventLogs::Event* event =
            system_info->mutable_event_logs()->add_event();

        event->set_level(level);
        event->set_time(enumerator.time());
        event->set_category(enumerator.category().toStdString());
        event->set_event_id(enumerator.eventId());
        event->set_source(enumerator.source().toStdString());
        event->set_description(enumerator.description().toStdString());

        enumerator.advance();
    }
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void fillLicensesInfo(proto::system_info::SystemInfo* system_info)
{
    readLicensesInformation(system_info->mutable_licenses());
}

//--------------------------------------------------------------------------------------------------
void fillApplicationsInfo(proto::system_info::SystemInfo* system_info)
{
    readApplicationsInformation(system_info->mutable_applications());
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillOpenFilesInfo(proto::system_info::SystemInfo* system_info)
{
    for (OpenFilesEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::OpenFiles::OpenFile* open_file =
            system_info->mutable_open_files()->add_open_file();

        open_file->set_id(enumerator.id());
        open_file->set_user_name(enumerator.userName().toStdString());
        open_file->set_lock_count(enumerator.lockCount());
        open_file->set_file_path(enumerator.filePath().toStdString());
    }
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void fillLocalUsersInfo(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::User> users = SysInfo::users();
    for (const SysInfo::User& user : users)
    {
        proto::system_info::LocalUsers::LocalUser* local_user =
            system_info->mutable_local_users()->add_local_user();

        local_user->set_name(user.name.toStdString());
        local_user->set_full_name(user.full_name.toStdString());
        local_user->set_home_dir(user.home_dir.toStdString());

        for (const SysInfo::UserGroup& group : user.groups)
            local_user->add_group()->set_name(group.name.toStdString());

        local_user->set_disabled(user.disabled);
        local_user->set_password_expired(user.password_expired);
        local_user->set_dont_expire_password(user.dont_expire_password);
        local_user->set_last_logon_time(user.last_logon_time);
    }
}

//--------------------------------------------------------------------------------------------------
void fillLocalUserGroupsInfo(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::UserGroup> groups = SysInfo::userGroups();
    for (const SysInfo::UserGroup& group : groups)
    {
        proto::system_info::LocalUserGroups::LocalUserGroup* local_group =
            system_info->mutable_local_user_groups()->add_local_user_group();

        local_group->set_name(group.name.toStdString());
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void fillProcessesInfo(proto::system_info::SystemInfo* system_info)
{
    ProcessMonitor process_monitor;

    process_monitor.processes(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ProcessMonitor::ProcessMap map = process_monitor.processes(false);
    for (auto it = map.cbegin(), it_end = map.cend(); it != it_end; ++it)
    {
        proto::system_info::Processes::Process* process_item =
            system_info->mutable_processes()->add_process();
        const ProcessMonitor::ProcessEntry& process = it.value();

        process_item->set_name(process.process_name.toStdString());
        process_item->set_pid(it.key());
        process_item->set_sid(process.session_id);
        process_item->set_user(process.user_name.toStdString());
        process_item->set_path(process.file_path.toStdString());
        process_item->set_cpu(process.cpu_ratio);
        process_item->set_memory(process.mem_private_working_set);
    }
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void fillSummaryInfo(proto::system_info::SystemInfo* system_info)
{
    fillComputer(system_info);
    fillOperatingSystem(system_info);
    fillProcessor(system_info);
    fillBios(system_info);
    fillMotherboard(system_info);
    fillMemory(system_info);
    fillDrives(system_info);
}

} // namespace

//--------------------------------------------------------------------------------------------------
void createSystemInfo(const proto::system_info::SystemInfoRequest& request,
                      proto::system_info::SystemInfo* system_info)
{
    if (request.category().empty())
    {
        fillSummaryInfo(system_info);
        return;
    }

    const std::string& category = request.category();

    LOG(INFO) << "Requested system info category:" << category;

    system_info->mutable_footer()->set_category(category);

    if (category == kSystemInfo_Summary)
    {
        fillSummaryInfo(system_info);
    }
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Devices)
    {
        fillDevices(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_VideoAdapters)
    {
        fillVideoAdapters(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Monitors)
    {
        fillMonitors(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Printers)
    {
        fillPrinters(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_PowerOptions)
    {
        fillPowerOptions(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Drivers)
    {
        fillDrivers(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Services)
    {
        fillServices(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_EnvironmentVariables)
    {
        fillEnvironmentVariables(system_info);
    }
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_EventLogs)
    {
        fillEventLogs(system_info, request.event_logs_data());
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_NetworkAdapters)
    {
        fillNetworkAdapters(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Routes)
    {
        fillRoutes(system_info);
    }
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Connections)
    {
        fillConnection(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_NetworkShares)
    {
        fillNetworkShares(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Licenses)
    {
        fillLicensesInfo(system_info);
    }
    else if (category == kSystemInfo_Applications)
    {
        fillApplicationsInfo(system_info);
    }
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_OpenFiles)
    {
        fillOpenFilesInfo(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_LocalUsers)
    {
        fillLocalUsersInfo(system_info);
    }
    else if (category == kSystemInfo_LocalUserGroups)
    {
        fillLocalUserGroupsInfo(system_info);
    }
#if defined(Q_OS_WINDOWS)
    else if (category == kSystemInfo_Processes)
    {
        fillProcessesInfo(system_info);
    }
#endif // defined(Q_OS_WINDOWS)
    else
    {
        LOG(ERROR) << "Unknown system info category:" << category;
    }
}
