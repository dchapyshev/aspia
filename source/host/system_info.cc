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

#include <QByteArray>
#include <QProcessEnvironment>
#include <QStorageInfo>

#include "base/edid.h"
#include "base/event_enumerator.h"
#include "base/license_reader.h"
#include "base/logging.h"
#include "base/smbios_parser.h"
#include "base/sys_info.h"
#include "base/net/net_utils.h"
#include "common/system_info_constants.h"
#include "host/process_monitor.h"
#include "proto/system_info.h"

namespace {

//--------------------------------------------------------------------------------------------------
void fillDevices(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Device> devices = SysInfo::devices();
    for (const SysInfo::Device& item : devices)
    {
        proto::system_info::WindowsDevices::Device* device =
            system_info->mutable_windows_devices()->add_device();

        device->set_friendly_name(item.friendly_name.toStdString());
        device->set_description(item.description.toStdString());
        device->set_driver_vendor(item.driver_vendor.toStdString());
        device->set_device_id(item.device_id.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillPrinters(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Printer> printers = SysInfo::printers();
    for (const SysInfo::Printer& item : printers)
    {
        proto::system_info::Printers::Printer* printer =
            system_info->mutable_printers()->add_printer();

        printer->set_name(item.name.toStdString());
        printer->set_is_default(item.is_default);
        printer->set_is_shared(item.is_shared);
        printer->set_port(item.port_name.toStdString());
        printer->set_driver(item.driver_name.toStdString());
        printer->set_jobs_count(static_cast<quint32>(item.jobs_count));
        printer->set_share_name(item.share_name.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillNetworkAdapters(proto::system_info::SystemInfo* system_info)
{
    const QList<NetUtils::Adapter> adapters = NetUtils::adapters();
    for (const NetUtils::Adapter& adapter : adapters)
    {
        proto::system_info::NetworkAdapters::Adapter* item =
            system_info->mutable_network_adapters()->add_adapter();

        item->set_adapter_name(adapter.adapter_name.toStdString());
        item->set_connection_name(adapter.connection_name.toStdString());
        item->set_iface(adapter.interface_type.toStdString());
        item->set_speed(adapter.speed);
        item->set_mac(adapter.mac.toStdString());
        item->set_dhcp_enabled(adapter.dhcp4_enabled);

        if (adapter.dhcp4_enabled && !adapter.dhcp4_server.isEmpty())
            item->add_dhcp()->append(adapter.dhcp4_server.toStdString());

        for (const QString& gateway : adapter.gateways)
            item->add_gateway()->assign(gateway.toStdString());

        for (const NetUtils::Adapter::Address& address : adapter.addresses)
        {
            proto::system_info::NetworkAdapters::Adapter::Address* item_address = item->add_address();
            item_address->set_ip(address.ip.toStdString());
            item_address->set_mask(address.mask.toStdString());
        }

        for (const QString& dns : adapter.dns_servers)
            item->add_dns()->assign(dns.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillNetworkShares(proto::system_info::SystemInfo* system_info)
{
    const QList<NetUtils::Share> shares = NetUtils::networkShares();
    for (const NetUtils::Share& item : shares)
    {
        proto::system_info::NetworkShares::Share* share =
            system_info->mutable_network_shares()->add_share();

        share->set_name(item.name.toStdString());
        share->set_description(item.description.toStdString());
        share->set_local_path(item.local_path.toStdString());
        share->set_type(item.type.toStdString());
        share->set_current_uses(item.current_uses);
        share->set_max_uses(item.max_uses);
    }
}

//--------------------------------------------------------------------------------------------------
void fillServices(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Service> services = SysInfo::services();
    for (const SysInfo::Service& service : services)
    {
        proto::system_info::Services::Service* item =
            system_info->mutable_services()->add_service();

        item->set_name(service.name.toStdString());
        item->set_display_name(service.display_name.toStdString());
        item->set_description(service.description.toStdString());

        switch (service.status)
        {
            case SysInfo::Service::Status::CONTINUE_PENDING:
                item->set_status(proto::system_info::Services::Service::STATUS_CONTINUE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSE_PENDING:
                item->set_status(proto::system_info::Services::Service::STATUS_PAUSE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSED:
                item->set_status(proto::system_info::Services::Service::STATUS_PAUSED);
                break;

            case SysInfo::Service::Status::RUNNING:
                item->set_status(proto::system_info::Services::Service::STATUS_RUNNING);
                break;

            case SysInfo::Service::Status::START_PENDING:
                item->set_status(proto::system_info::Services::Service::STATUS_START_PENDING);
                break;

            case SysInfo::Service::Status::STOP_PENDING:
                item->set_status(proto::system_info::Services::Service::STATUS_STOP_PENDING);
                break;

            case SysInfo::Service::Status::STOPPED:
                item->set_status(proto::system_info::Services::Service::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::system_info::Services::Service::STATUS_UNKNOWN);
                break;
        }

        switch (service.startup_type)
        {
            case SysInfo::Service::StartupType::AUTO_START:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_AUTO_START);
                break;

            case SysInfo::Service::StartupType::DEMAND_START:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_DEMAND_START);
                break;

            case SysInfo::Service::StartupType::DISABLED:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_DISABLED);
                break;

            case SysInfo::Service::StartupType::BOOT_START:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_BOOT_START);
                break;

            case SysInfo::Service::StartupType::SYSTEM_START:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::system_info::Services::Service::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(service.binary_path.toStdString());
        item->set_start_name(service.start_name.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillDrivers(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Service> drivers = SysInfo::drivers();
    for (const SysInfo::Service& driver : drivers)
    {
        proto::system_info::Drivers::Driver* item = system_info->mutable_drivers()->add_driver();

        item->set_name(driver.name.toStdString());
        item->set_display_name(driver.display_name.toStdString());
        item->set_description(driver.description.toStdString());

        switch (driver.status)
        {
            case SysInfo::Service::Status::CONTINUE_PENDING:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_CONTINUE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSE_PENDING:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_PAUSE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSED:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_PAUSED);
                break;

            case SysInfo::Service::Status::RUNNING:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_RUNNING);
                break;

            case SysInfo::Service::Status::START_PENDING:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_START_PENDING);
                break;

            case SysInfo::Service::Status::STOP_PENDING:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_STOP_PENDING);
                break;

            case SysInfo::Service::Status::STOPPED:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::system_info::Drivers::Driver::STATUS_UNKNOWN);
                break;
        }

        switch (driver.startup_type)
        {
            case SysInfo::Service::StartupType::AUTO_START:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_AUTO_START);
                break;

            case SysInfo::Service::StartupType::DEMAND_START:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_DEMAND_START);
                break;

            case SysInfo::Service::StartupType::DISABLED:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_DISABLED);
                break;

            case SysInfo::Service::StartupType::BOOT_START:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_BOOT_START);
                break;

            case SysInfo::Service::StartupType::SYSTEM_START:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::system_info::Drivers::Driver::STARTUP_TYPE_UNKNOWN);
                break;
        }

        item->set_binary_path(driver.binary_path.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillMonitors(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Monitor> monitors = SysInfo::monitors();
    for (const SysInfo::Monitor& item : monitors)
    {
        Edid edid(item.edid);
        if (!edid.isValid())
        {
            LOG(INFO) << "No EDID information for monitor";
            continue;
        }

        proto::system_info::Monitors::Monitor* monitor =
            system_info->mutable_monitors()->add_monitor();

        monitor->set_system_name(item.system_name.toStdString());
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

//--------------------------------------------------------------------------------------------------
void fillConnection(proto::system_info::SystemInfo* system_info)
{
    const QList<NetUtils::Connection> connections = NetUtils::connections();
    for (const NetUtils::Connection& item : connections)
    {
        proto::system_info::Connections::Connection* connection =
            system_info->mutable_connections()->add_connection();

        connection->set_protocol(item.protocol.toStdString());
        connection->set_process_name(item.process_name.toStdString());
        connection->set_local_address(item.local_address.toStdString());
        connection->set_remote_address(item.remote_address.toStdString());
        connection->set_local_port(item.local_port);
        connection->set_remote_port(item.remote_port);
        connection->set_state(item.state.toStdString());
    }
}

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
    const QStringList list = QProcessEnvironment::systemEnvironment().toStringList();

    for (const auto& item : std::as_const(list))
    {
        // Split on the first '=' only so values may contain '='; skip entries with no name.
        const qsizetype pos = item.indexOf('=');
        if (pos <= 0)
            continue;

        proto::system_info::EnvironmentVariables::Variable* variable =
            system_info->mutable_env_vars()->add_variable();
        variable->set_name(item.left(pos).toStdString());
        variable->set_value(item.mid(pos + 1).toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillVideoAdapters(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::VideoAdapter> adapters = SysInfo::videoAdapters();
    for (const SysInfo::VideoAdapter& item : adapters)
    {
        proto::system_info::VideoAdapters::Adapter* adapter =
            system_info->mutable_video_adapters()->add_adapter();

        adapter->set_description(item.description.toStdString());
        adapter->set_adapter_string(item.adapter_string.toStdString());
        adapter->set_bios_string(item.bios_string.toStdString());
        adapter->set_chip_type(item.chip_type.toStdString());
        adapter->set_dac_type(item.dac_type.toStdString());
        adapter->set_driver_date(item.driver_date.toStdString());
        adapter->set_driver_version(item.driver_version.toStdString());
        adapter->set_driver_provider(item.driver_provider.toStdString());
        adapter->set_memory_size(item.memory_size);
    }
}

//--------------------------------------------------------------------------------------------------
void fillPowerOptions(proto::system_info::SystemInfo* system_info)
{
    const SysInfo::PowerOptions power = SysInfo::powerOptions();
    proto::system_info::PowerOptions* power_options = system_info->mutable_power_options();

    switch (power.power_source)
    {
        case SysInfo::PowerOptions::PowerSource::DC_BATTERY:
            power_options->set_power_source(proto::system_info::PowerOptions::POWER_SOURCE_DC_BATTERY);
            break;

        case SysInfo::PowerOptions::PowerSource::AC_LINE:
            power_options->set_power_source(proto::system_info::PowerOptions::POWER_SOURCE_AC_LINE);
            break;

        default:
            break;
    }

    switch (power.battery_status)
    {
        case SysInfo::PowerOptions::BatteryStatus::HIGH:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_HIGH);
            break;

        case SysInfo::PowerOptions::BatteryStatus::LOW:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_LOW);
            break;

        case SysInfo::PowerOptions::BatteryStatus::CRITICAL:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_CRITICAL);
            break;

        case SysInfo::PowerOptions::BatteryStatus::CHARGING:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_CHARGING);
            break;

        case SysInfo::PowerOptions::BatteryStatus::NO_BATTERY:
            power_options->set_battery_status(proto::system_info::PowerOptions::BATTERY_STATUS_NO_BATTERY);
            break;

        default:
            break;
    }

    power_options->set_battery_life_percent(power.battery_life_percent);
    power_options->set_full_battery_life_time(power.full_battery_life_time);
    power_options->set_remaining_battery_life_time(power.remaining_battery_life_time);

    for (const SysInfo::PowerOptions::Battery& item : power.batteries)
    {
        proto::system_info::PowerOptions::Battery* battery = power_options->add_battery();
        battery->set_device_name(item.device_name.toStdString());
        battery->set_manufacturer(item.manufacturer.toStdString());
        battery->set_manufacture_date(item.manufacture_date.toStdString());
        battery->set_unique_id(item.unique_id.toStdString());
        battery->set_serial_number(item.serial_number.toStdString());
        battery->set_temperature(item.temperature.toStdString());
        battery->set_design_capacity(item.design_capacity);
        battery->set_type(item.type.toStdString());
        battery->set_full_charged_capacity(item.full_charged_capacity);
        battery->set_depreciation(item.depreciation);
        battery->set_current_capacity(item.current_capacity);
        battery->set_voltage(item.voltage);
        battery->set_state(item.state);
    }
}

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
                if (!bios_table.isValid())
                    continue;

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

    EventEnumerator::Direction direction =
        (data.direction() == proto::system_info::EventLogsData::DIRECTION_NEWER) ?
            EventEnumerator::Direction::NEWER : EventEnumerator::Direction::OLDER;

    std::unique_ptr<EventEnumerator> enumerator = EventEnumerator::create(
        log_name, QByteArray::fromStdString(data.cursor()), direction, data.record_count());
    if (!enumerator)
        return;

    system_info->mutable_event_logs()->set_type(data.type());

    while (!enumerator->isAtEnd())
    {
        proto::system_info::EventLogs::Event::Level level;
        switch (enumerator->type())
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
        event->set_time(enumerator->time());
        event->set_event_id(enumerator->eventId());
        event->set_source(enumerator->source().toStdString());
        event->set_description(enumerator->description().toStdString());

        enumerator->advance();
    }

    proto::system_info::EventLogs* event_logs = system_info->mutable_event_logs();
    event_logs->set_first_cursor(enumerator->firstCursor().toStdString());
    event_logs->set_last_cursor(enumerator->lastCursor().toStdString());
    event_logs->set_at_newest(enumerator->atNewest());
    event_logs->set_at_oldest(enumerator->atOldest());
}

//--------------------------------------------------------------------------------------------------
void fillLicensesInfo(proto::system_info::SystemInfo* system_info)
{
    readLicensesInformation(system_info->mutable_licenses());
}

//--------------------------------------------------------------------------------------------------
void fillApplicationsInfo(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::Application> applications = SysInfo::applications();
    for (const SysInfo::Application& application : applications)
    {
        proto::system_info::Applications::Application* item =
            system_info->mutable_applications()->add_application();

        item->set_name(application.name.toStdString());
        item->set_version(application.version.toStdString());
        item->set_publisher(application.publisher.toStdString());
    }
}

//--------------------------------------------------------------------------------------------------
void fillOpenFilesInfo(proto::system_info::SystemInfo* system_info)
{
    const QList<NetUtils::OpenFile> open_files = NetUtils::openFiles();
    for (const NetUtils::OpenFile& item : open_files)
    {
        proto::system_info::OpenFiles::OpenFile* open_file =
            system_info->mutable_open_files()->add_open_file();

        open_file->set_id(item.id);
        open_file->set_user_name(item.user_name.toStdString());
        open_file->set_lock_count(item.lock_count);
        open_file->set_file_path(item.file_path.toStdString());
    }
}

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

//--------------------------------------------------------------------------------------------------
void fillProcessesInfo(proto::system_info::SystemInfo* system_info)
{
    std::unique_ptr<ProcessMonitor> process_monitor = ProcessMonitor::create();
    if (!process_monitor)
        return;

    const ProcessMonitor::ProcessMap& map = process_monitor->processes(true);
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
        process_item->set_memory(process.mem_private_working_set);
    }
}

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
    else if (category == kSystemInfo_Devices)
    {
        fillDevices(system_info);
    }
    else if (category == kSystemInfo_VideoAdapters)
    {
        fillVideoAdapters(system_info);
    }
    else if (category == kSystemInfo_Monitors)
    {
        fillMonitors(system_info);
    }
    else if (category == kSystemInfo_Printers)
    {
        fillPrinters(system_info);
    }
    else if (category == kSystemInfo_PowerOptions)
    {
        fillPowerOptions(system_info);
    }
    else if (category == kSystemInfo_Drivers)
    {
        fillDrivers(system_info);
    }
    else if (category == kSystemInfo_Services)
    {
        fillServices(system_info);
    }
    else if (category == kSystemInfo_EnvironmentVariables)
    {
        fillEnvironmentVariables(system_info);
    }
    else if (category == kSystemInfo_EventLogs)
    {
        fillEventLogs(system_info, request.event_logs_data());
    }
    else if (category == kSystemInfo_NetworkAdapters)
    {
        fillNetworkAdapters(system_info);
    }
    else if (category == kSystemInfo_Routes)
    {
        fillRoutes(system_info);
    }
    else if (category == kSystemInfo_Connections)
    {
        fillConnection(system_info);
    }
    else if (category == kSystemInfo_NetworkShares)
    {
        fillNetworkShares(system_info);
    }
    else if (category == kSystemInfo_Licenses)
    {
        fillLicensesInfo(system_info);
    }
    else if (category == kSystemInfo_Applications)
    {
        fillApplicationsInfo(system_info);
    }
    else if (category == kSystemInfo_OpenFiles)
    {
        fillOpenFilesInfo(system_info);
    }
    else if (category == kSystemInfo_LocalUsers)
    {
        fillLocalUsersInfo(system_info);
    }
    else if (category == kSystemInfo_LocalUserGroups)
    {
        fillLocalUserGroupsInfo(system_info);
    }
    else if (category == kSystemInfo_Processes)
    {
        fillProcessesInfo(system_info);
    }
    else
    {
        LOG(ERROR) << "Unknown system info category:" << category;
    }
}
