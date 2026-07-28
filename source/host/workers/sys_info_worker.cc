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

#include "host/workers/sys_info_worker.h"

#include <QByteArray>
#include <QHash>
#include <QProcessEnvironment>
#include <QStorageInfo>
#include <QStringList>

#include <atomic>

#include "base/edid.h"
#include "base/event_enumerator.h"
#include "base/license_reader.h"
#include "base/logging.h"
#include "base/physical_drive_reader.h"
#include "base/serialization.h"
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

    for (const auto& item : list)
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
QString cacheSupportedSram(const SmbiosCache& cache)
{
    QStringList types;

    if (cache.supportsNonBurst())
        types << "Non-Burst";
    if (cache.supportsBurst())
        types << "Burst";
    if (cache.supportsPipelineBurst())
        types << "Pipeline Burst";
    if (cache.supportsSynchronous())
        types << "Synchronous";
    if (cache.supportsAsynchronous())
        types << "Asynchronous";

    return types.join(", ");
}

//--------------------------------------------------------------------------------------------------
// The address of a device on the PCI bus, in the notation the operating systems use. The system
// slot and the on-board device report it the same way.
template <class T>
QString busAddress(const T& table)
{
    if (!table.hasBusAddress())
        return QString();

    return QString("%1:%2:%3.%4").arg(table.segmentGroupNumber(), 4, 16, QChar('0'))
                                 .arg(table.busNumber(), 2, 16, QChar('0'))
                                 .arg(table.deviceNumber(), 2, 16, QChar('0'))
                                 .arg(table.functionNumber());
}

//--------------------------------------------------------------------------------------------------
// Names of the tables other tables point at by handle: the locator of a memory device and the use
// of a memory array. A table may come before the one it points at, so the names are collected
// before the tables are read. Handles are unique within the whole set of tables.
QHash<quint16, QString> referencedTableNames(const QByteArray& dump)
{
    QHash<quint16, QString> result;

    for (SmbiosTableEnumerator enumerator(dump); !enumerator.isAtEnd(); enumerator.advance())
    {
        const SmbiosTable* table = enumerator.table();

        switch (table->type)
        {
            case SMBIOS_TABLE_TYPE_MEMORY_ARRAY:
            {
                SmbiosMemoryArray memory_array(table);
                if (memory_array.isValid())
                    result.insert(table->handle, memory_array.use());
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_DEVICE:
            {
                SmbiosMemoryDevice memory_device(table);
                if (memory_device.isValid())
                    result.insert(table->handle, memory_device.location());
            }
            break;

            default:
                break;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void fillDmi(proto::system_info::SystemInfo* system_info)
{
    proto::system_info::Dmi* dmi = system_info->mutable_dmi();
    const QByteArray dump = SysInfo::smbiosDump();
    const QHash<quint16, QString> table_names = referencedTableNames(dump);
    SmbiosTableEnumerator enumerator(dump);
    quint32 structure_count = 0;

    for (; !enumerator.isAtEnd(); enumerator.advance())
    {
        const SmbiosTable* table = enumerator.table();
        ++structure_count;

        switch (table->type)
        {
            case SMBIOS_TABLE_TYPE_BIOS:
            {
                SmbiosBios bios_table(table);
                if (!bios_table.isValid())
                    continue;

                proto::system_info::Dmi::Bios* bios = dmi->add_bios();

                bios->set_vendor(bios_table.vendor().toStdString());
                bios->set_version(bios_table.version().toStdString());
                bios->set_release_date(bios_table.releaseDate().toStdString());
                bios->set_address(bios_table.address());
                bios->set_rom_size(bios_table.romSize());
                bios->set_revision(bios_table.revision().toStdString());
                bios->set_firmware_revision(bios_table.firmwareRevision().toStdString());

                const QStringList characteristics = bios_table.characteristics();
                for (const QString& characteristic : characteristics)
                    bios->add_characteristic(characteristic.toStdString());
            }
            break;

            case SMBIOS_TABLE_TYPE_BASEBOARD:
            {
                SmbiosBaseboard baseboard_table(table);
                if (!baseboard_table.isValid())
                    continue;

                proto::system_info::Dmi::Baseboard* baseboard = dmi->add_baseboard();

                baseboard->set_manufacturer(baseboard_table.manufacturer().toStdString());
                baseboard->set_product(baseboard_table.product().toStdString());
                baseboard->set_version(baseboard_table.version().toStdString());
                baseboard->set_serial_number(baseboard_table.serialNumber().toStdString());
                baseboard->set_asset_tag(baseboard_table.assetTag().toStdString());
                baseboard->set_location(baseboard_table.location().toStdString());
                baseboard->set_type(baseboard_table.type().toStdString());
                baseboard->set_hosting_board(baseboard_table.isHostingBoard());
                baseboard->set_requires_daughter_board(baseboard_table.requiresDaughterBoard());
                baseboard->set_removable(baseboard_table.isRemovable());
                baseboard->set_replaceable(baseboard_table.isReplaceable());
                baseboard->set_hot_swappable(baseboard_table.isHotSwappable());
            }
            break;

            case SMBIOS_TABLE_TYPE_CHASSIS:
            {
                SmbiosChassis chassis_table(table);
                if (!chassis_table.isValid())
                    continue;

                proto::system_info::Dmi::Chassis* chassis = dmi->add_chassis();

                chassis->set_manufacturer(chassis_table.manufacturer().toStdString());
                chassis->set_version(chassis_table.version().toStdString());
                chassis->set_serial_number(chassis_table.serialNumber().toStdString());
                chassis->set_asset_tag(chassis_table.assetTag().toStdString());
                chassis->set_sku_number(chassis_table.skuNumber().toStdString());
                chassis->set_type(chassis_table.type().toStdString());
                chassis->set_boot_up_state(chassis_table.bootUpState().toStdString());
                chassis->set_power_supply_state(chassis_table.powerSupplyState().toStdString());
                chassis->set_thermal_state(chassis_table.thermalState().toStdString());
                chassis->set_security_status(chassis_table.securityStatus().toStdString());
                chassis->set_lock_present(chassis_table.isLockPresent());
                chassis->set_height(chassis_table.height());
                chassis->set_power_cords(chassis_table.powerCords());
            }
            break;

            case SMBIOS_TABLE_TYPE_PROCESSOR:
            {
                SmbiosProcessor processor_table(table);
                if (!processor_table.isValid())
                    continue;

                proto::system_info::Dmi::Processor* processor = dmi->add_processor();

                processor->set_populated(processor_table.isPopulated());
                processor->set_manufacturer(processor_table.manufacturer().toStdString());
                processor->set_version(processor_table.version().toStdString());
                processor->set_family(processor_table.family().toStdString());
                processor->set_type(processor_table.type().toStdString());
                processor->set_status(processor_table.status().toStdString());
                processor->set_socket_designation(
                    processor_table.socketDesignation().toStdString());
                processor->set_socket(processor_table.upgrade().toStdString());
                processor->set_socket_type(processor_table.socketType().toStdString());
                processor->set_serial_number(processor_table.serialNumber().toStdString());
                processor->set_asset_tag(processor_table.assetTag().toStdString());
                processor->set_part_number(processor_table.partNumber().toStdString());
                processor->set_id(processor_table.id());
                processor->set_voltage(processor_table.voltage());
                processor->set_external_clock(processor_table.externalClock());
                processor->set_max_speed(processor_table.maxSpeed());
                processor->set_current_speed(processor_table.currentSpeed());
                processor->set_core_count(processor_table.coreCount());
                processor->set_core_enabled(processor_table.coreEnabled());
                processor->set_thread_count(processor_table.threadCount());
                processor->set_thread_enabled(processor_table.threadEnabled());
                processor->set_support_64bit(processor_table.is64Bit());
                processor->set_support_multi_core(processor_table.isMultiCore());
                processor->set_support_hardware_thread(processor_table.isHardwareThread());
                processor->set_support_execute_protection(processor_table.isExecuteProtection());
                processor->set_support_enhanced_virtualization(
                    processor_table.isEnhancedVirtualization());
                processor->set_support_power_control(
                    processor_table.isPowerPerformanceControl());
            }
            break;

            case SMBIOS_TABLE_TYPE_CACHE:
            {
                SmbiosCache cache_table(table);
                if (!cache_table.isValid())
                    continue;

                proto::system_info::Dmi::Cache* cache = dmi->add_cache();

                cache->set_designation(cache_table.designation().toStdString());
                cache->set_location(cache_table.location().toStdString());
                cache->set_mode(cache_table.mode().toStdString());
                cache->set_type(cache_table.type().toStdString());
                cache->set_sram_type(cache_table.currentSramType().toStdString());
                cache->set_supported_sram_type(cacheSupportedSram(cache_table).toStdString());
                cache->set_error_correction_type(
                    cache_table.errorCorrectionType().toStdString());
                cache->set_associativity(cache_table.associativity().toStdString());
                cache->set_level(cache_table.level());
                cache->set_max_size(cache_table.maxSize());
                cache->set_current_size(cache_table.currentSize());
                cache->set_speed(cache_table.speed());
                cache->set_enabled(cache_table.isEnabled());
                cache->set_socketed(cache_table.isSocketed());
            }
            break;

            case SMBIOS_TABLE_TYPE_PORT_CONNECTOR:
            {
                SmbiosPortConnector port_table(table);
                if (!port_table.isValid())
                    continue;

                proto::system_info::Dmi::PortConnector* port = dmi->add_port_connector();

                port->set_internal_designator(port_table.internalDesignator().toStdString());
                port->set_internal_connector_type(
                    port_table.internalConnectorType().toStdString());
                port->set_external_designator(port_table.externalDesignator().toStdString());
                port->set_external_connector_type(
                    port_table.externalConnectorType().toStdString());
                port->set_type(port_table.type().toStdString());
            }
            break;

            case SMBIOS_TABLE_TYPE_SYSTEM_SLOT:
            {
                SmbiosSystemSlot slot_table(table);
                if (!slot_table.isValid())
                    continue;

                proto::system_info::Dmi::SystemSlot* slot = dmi->add_system_slot();

                slot->set_designation(slot_table.designation().toStdString());
                slot->set_type(slot_table.type().toStdString());
                slot->set_data_bus_width(slot_table.dataBusWidth().toStdString());
                slot->set_usage(slot_table.usage().toStdString());
                slot->set_length(slot_table.length().toStdString());
                slot->set_bus_address(busAddress(slot_table).toStdString());
                slot->set_id(slot_table.id());
                slot->set_provides_5_volts(slot_table.provides5Volts());
                slot->set_provides_3_volts(slot_table.provides3Volts());
                slot->set_shared(slot_table.isShared());
                slot->set_supports_pme(slot_table.supportsPme());
                slot->set_supports_hot_plug(slot_table.supportsHotPlug());
                slot->set_supports_smbus(slot_table.supportsSmbus());
                slot->set_supports_bifurcation(slot_table.supportsBifurcation());
            }
            break;

            case SMBIOS_TABLE_TYPE_ONBOARD_DEVICE:
            {
                SmbiosOnBoardDevices devices_table(table);
                if (!devices_table.isValid())
                    continue;

                // The legacy table keeps a list of devices, all of them without an address.
                for (int i = 0; i < devices_table.count(); ++i)
                {
                    proto::system_info::Dmi::OnBoardDevice* device = dmi->add_on_board_device();

                    device->set_description(devices_table.description(i).toStdString());
                    device->set_type(devices_table.type(i).toStdString());
                    device->set_enabled(devices_table.isEnabled(i));
                }
            }
            break;

            case SMBIOS_TABLE_TYPE_ONBOARD_DEVICE_EXT:
            {
                SmbiosOnBoardDeviceExt device_table(table);
                if (!device_table.isValid())
                    continue;

                proto::system_info::Dmi::OnBoardDevice* device = dmi->add_on_board_device();

                device->set_description(device_table.designation().toStdString());
                device->set_type(device_table.type().toStdString());
                device->set_bus_address(busAddress(device_table).toStdString());
                device->set_instance(device_table.typeInstance());
                device->set_enabled(device_table.isEnabled());
            }
            break;

            case SMBIOS_TABLE_TYPE_OEM_STRINGS:
            case SMBIOS_TABLE_TYPE_CONFIGURATION_OPTION:
            {
                SmbiosStringList strings_table(table);
                if (!strings_table.isValid())
                    continue;

                // Firmware often declares strings it does not store, so the empty ones are
                // dropped instead of showing up as blank rows.
                for (int i = 0; i < strings_table.count(); ++i)
                {
                    const QString string = strings_table.string(i);
                    if (string.isEmpty())
                        continue;

                    if (table->type == SMBIOS_TABLE_TYPE_OEM_STRINGS)
                        dmi->add_oem_string(string.toStdString());
                    else
                        dmi->add_configuration_option(string.toStdString());
                }
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_ERROR:
            {
                SmbiosMemoryError error_table(table);
                if (!error_table.isValid())
                    continue;

                proto::system_info::Dmi::MemoryError* error = dmi->add_memory_error();

                error->set_type(error_table.type().toStdString());
                error->set_granularity(error_table.granularity().toStdString());
                error->set_operation(error_table.operation().toStdString());
                error->set_syndrome(error_table.vendorSyndrome());
                error->set_array_address(error_table.arrayErrorAddress());
                error->set_device_address(error_table.deviceErrorAddress());
                error->set_resolution(error_table.errorResolution());
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_ARRAY_ADDRESS:
            {
                SmbiosMemoryArrayAddress array_address_table(table);
                if (!array_address_table.isValid())
                    continue;

                proto::system_info::Dmi::MemoryArrayAddress* array_address =
                    dmi->add_memory_array_address();

                array_address->set_array(
                    table_names.value(array_address_table.arrayHandle()).toStdString());
                array_address->set_start_address(array_address_table.startAddress());
                array_address->set_end_address(array_address_table.endAddress());
                array_address->set_size(array_address_table.size());
                array_address->set_partition_width(array_address_table.partitionWidth());
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_DEVICE_ADDR:
            {
                SmbiosMemoryDeviceAddress address_table(table);
                if (!address_table.isValid())
                    continue;

                proto::system_info::Dmi::MemoryDeviceAddress* address =
                    dmi->add_memory_device_address();

                address->set_device(
                    table_names.value(address_table.deviceHandle()).toStdString());
                address->set_start_address(address_table.startAddress());
                address->set_end_address(address_table.endAddress());
                address->set_size(address_table.size());
                address->set_row_position(address_table.rowPosition());
                address->set_interleave_position(address_table.interleavePosition());
                address->set_interleave_depth(address_table.interleaveDepth());
            }
            break;

            case SMBIOS_TABLE_TYPE_VOLTAGE_PROBE:
            case SMBIOS_TABLE_TYPE_TEMPERATURE_PROBE:
            case SMBIOS_TABLE_TYPE_CURRENT_PROBE:
            {
                SmbiosProbe probe_table(table);
                if (!probe_table.isValid())
                    continue;

                proto::system_info::Dmi::Probe* probe = nullptr;

                // The three tables share their layout, the unit of their values is what the field
                // the message lands in tells.
                switch (table->type)
                {
                    case SMBIOS_TABLE_TYPE_VOLTAGE_PROBE:
                        probe = dmi->add_voltage_probe();
                        break;

                    case SMBIOS_TABLE_TYPE_TEMPERATURE_PROBE:
                        probe = dmi->add_temperature_probe();
                        break;

                    default:
                        probe = dmi->add_current_probe();
                        break;
                }

                probe->set_description(probe_table.description().toStdString());
                probe->set_location(probe_table.location().toStdString());
                probe->set_status(probe_table.status().toStdString());
                probe->set_max_value(probe_table.maxValue());
                probe->set_min_value(probe_table.minValue());
                probe->set_nominal_value(probe_table.nominalValue());
                probe->set_tolerance(probe_table.tolerance());
                probe->set_resolution(probe_table.resolution());
                probe->set_accuracy(probe_table.accuracy());
            }
            break;

            case SMBIOS_TABLE_TYPE_COOLING_DEVICE:
            {
                SmbiosCoolingDevice cooling_table(table);
                if (!cooling_table.isValid())
                    continue;

                proto::system_info::Dmi::CoolingDevice* cooling = dmi->add_cooling_device();

                cooling->set_description(cooling_table.description().toStdString());
                cooling->set_type(cooling_table.type().toStdString());
                cooling->set_status(cooling_table.status().toStdString());
                cooling->set_unit_group(cooling_table.unitGroup());
                cooling->set_nominal_speed(cooling_table.nominalSpeed());
            }
            break;

            case SMBIOS_TABLE_TYPE_SYSTEM_BOOT:
            {
                SmbiosSystemBoot boot_table(table);
                if (!boot_table.isValid())
                    continue;

                dmi->set_boot_status(boot_table.status().toStdString());
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_ARRAY:
            {
                SmbiosMemoryArray memory_array_table(table);
                if (!memory_array_table.isValid())
                    continue;

                proto::system_info::Dmi::MemoryArray* memory_array = dmi->add_memory_array();

                memory_array->set_location(memory_array_table.location().toStdString());
                memory_array->set_use(memory_array_table.use().toStdString());
                memory_array->set_error_correction(
                    memory_array_table.errorCorrection().toStdString());
                memory_array->set_max_capacity(memory_array_table.maxCapacity());
                memory_array->set_device_count(memory_array_table.deviceCount());
            }
            break;

            case SMBIOS_TABLE_TYPE_MEMORY_DEVICE:
            {
                SmbiosMemoryDevice memory_device_table(table);
                if (!memory_device_table.isValid())
                    continue;

                proto::system_info::Dmi::MemoryDevice* memory_device = dmi->add_memory_device();

                memory_device->set_present(memory_device_table.isPresent());
                memory_device->set_location(memory_device_table.location().toStdString());

                if (!memory_device_table.isPresent())
                    continue;

                memory_device->set_bank(memory_device_table.bankLocator().toStdString());
                memory_device->set_manufacturer(
                    memory_device_table.manufacturer().toStdString());
                memory_device->set_serial_number(
                    memory_device_table.serialNumber().toStdString());
                memory_device->set_asset_tag(memory_device_table.assetTag().toStdString());
                memory_device->set_part_number(memory_device_table.partNumber().toStdString());
                memory_device->set_firmware_version(
                    memory_device_table.firmwareVersion().toStdString());
                memory_device->set_size(memory_device_table.size());
                memory_device->set_type(memory_device_table.type().toStdString());
                memory_device->set_type_detail(
                    memory_device_table.typeDetail().join(", ").toStdString());
                memory_device->set_form_factor(memory_device_table.formFactor().toStdString());
                memory_device->set_technology(memory_device_table.technology().toStdString());
                memory_device->set_speed(memory_device_table.speed());
                memory_device->set_configured_speed(memory_device_table.configuredSpeed());
                memory_device->set_total_width(memory_device_table.totalWidth());
                memory_device->set_data_width(memory_device_table.dataWidth());
                memory_device->set_rank(memory_device_table.rank());
                memory_device->set_min_voltage(memory_device_table.minVoltage());
                memory_device->set_max_voltage(memory_device_table.maxVoltage());
                memory_device->set_configured_voltage(memory_device_table.configuredVoltage());
                memory_device->set_non_volatile_size(memory_device_table.nonVolatileSize());
                memory_device->set_volatile_size(memory_device_table.volatileSize());
                memory_device->set_cache_size(memory_device_table.cacheSize());
                memory_device->set_logical_size(memory_device_table.logicalSize());
            }
            break;

            case SMBIOS_TABLE_TYPE_TPM_DEVICE:
            {
                SmbiosTpmDevice tpm_table(table);
                if (!tpm_table.isValid())
                    continue;

                proto::system_info::Dmi::TpmDevice* tpm = dmi->add_tpm_device();

                tpm->set_vendor_id(tpm_table.vendorId().toStdString());
                tpm->set_spec_version(tpm_table.specVersion().toStdString());
                tpm->set_firmware_version(tpm_table.firmwareVersion().toStdString());
                tpm->set_description(tpm_table.description().toStdString());
                tpm->set_configurable_by_firmware(tpm_table.isFamilyConfigurableByFirmware());
                tpm->set_configurable_by_software(tpm_table.isFamilyConfigurableBySoftware());
                tpm->set_configurable_by_oem(tpm_table.isFamilyConfigurableByOem());
            }
            break;

            default:
                break;
        }
    }

    // Firmware without the tables at all leaves nothing to tell about them either.
    if (!structure_count)
        return;

    proto::system_info::Dmi::Misc* misc = dmi->mutable_misc();

    misc->set_smbios_version(QString("%1.%2").arg(enumerator.majorVersion())
                                             .arg(enumerator.minorVersion()).toStdString());
    misc->set_structure_count(structure_count);
    misc->set_structure_size(enumerator.length());
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
proto::system_info::PhysicalDrives::Drive::BusType busType(PhysicalDriveReader::BusType bus_type)
{
    using ProtoDrive = proto::system_info::PhysicalDrives::Drive;

    switch (bus_type)
    {
        case PhysicalDriveReader::BusType::SCSI:    return ProtoDrive::BUS_TYPE_SCSI;
        case PhysicalDriveReader::BusType::ATAPI:   return ProtoDrive::BUS_TYPE_ATAPI;
        case PhysicalDriveReader::BusType::ATA:     return ProtoDrive::BUS_TYPE_ATA;
        case PhysicalDriveReader::BusType::IEEE1394: return ProtoDrive::BUS_TYPE_IEEE1394;
        case PhysicalDriveReader::BusType::SSA:     return ProtoDrive::BUS_TYPE_SSA;
        case PhysicalDriveReader::BusType::FIBRE:   return ProtoDrive::BUS_TYPE_FIBRE;
        case PhysicalDriveReader::BusType::USB:     return ProtoDrive::BUS_TYPE_USB;
        case PhysicalDriveReader::BusType::RAID:    return ProtoDrive::BUS_TYPE_RAID;
        case PhysicalDriveReader::BusType::ISCSI:   return ProtoDrive::BUS_TYPE_ISCSI;
        case PhysicalDriveReader::BusType::SAS:     return ProtoDrive::BUS_TYPE_SAS;
        case PhysicalDriveReader::BusType::SATA:    return ProtoDrive::BUS_TYPE_SATA;
        case PhysicalDriveReader::BusType::SD:      return ProtoDrive::BUS_TYPE_SD;
        case PhysicalDriveReader::BusType::MMC:     return ProtoDrive::BUS_TYPE_MMC;
        case PhysicalDriveReader::BusType::VIRTUAL: return ProtoDrive::BUS_TYPE_VIRTUAL;
        case PhysicalDriveReader::BusType::NVME:    return ProtoDrive::BUS_TYPE_NVME;
        case PhysicalDriveReader::BusType::SPACES:  return ProtoDrive::BUS_TYPE_SPACES;
        case PhysicalDriveReader::BusType::SCM:     return ProtoDrive::BUS_TYPE_SCM;
        case PhysicalDriveReader::BusType::UFS:     return ProtoDrive::BUS_TYPE_UFS;
        case PhysicalDriveReader::BusType::NVME_OF: return ProtoDrive::BUS_TYPE_NVME_OF;

        case PhysicalDriveReader::BusType::FILE_BACKED_VIRTUAL:
            return ProtoDrive::BUS_TYPE_FILE_BACKED_VIRTUAL;

        default:
            return ProtoDrive::BUS_TYPE_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
proto::system_info::PhysicalDrives::Drive::MediaType mediaType(
    PhysicalDriveReader::MediaType media_type)
{
    using ProtoDrive = proto::system_info::PhysicalDrives::Drive;

    switch (media_type)
    {
        case PhysicalDriveReader::MediaType::ROTATING:    return ProtoDrive::MEDIA_TYPE_ROTATING;
        case PhysicalDriveReader::MediaType::SOLID_STATE: return ProtoDrive::MEDIA_TYPE_SOLID_STATE;
        default:                                          return ProtoDrive::MEDIA_TYPE_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
void fillPhysicalDrives(proto::system_info::SystemInfo* system_info)
{
    const QList<SysInfo::PhysicalDrive> drives = SysInfo::physicalDrives();

    for (const SysInfo::PhysicalDrive& item : drives)
    {
        proto::system_info::PhysicalDrives::Drive* drive =
            system_info->mutable_physical_drives()->add_drive();

        drive->set_path(item.path.toStdString());
        drive->set_model(item.model.toStdString());
        drive->set_serial_number(item.serial_number.toStdString());
        drive->set_firmware_revision(item.firmware_revision.toStdString());
        drive->set_bus_type(busType(item.bus_type));
        drive->set_size(item.size);
        drive->set_rotation_rate(item.rotation_rate);
        drive->set_buffer_size(item.buffer_size);
        drive->set_removable(item.removable);
        drive->set_media_type(mediaType(item.media_type));

        for (const AtaSmart::Attribute& attribute : item.ata_smart)
        {
            proto::system_info::PhysicalDrives::Drive::SmartAttribute* smart =
                drive->add_smart_attribute();

            smart->set_id(attribute.id);
            smart->set_status_flags(attribute.status_flags);
            smart->set_value(attribute.value);
            smart->set_worst_value(attribute.worst_value);
            smart->set_threshold(attribute.threshold);
            smart->set_raw(attribute.raw);
        }

        if (!item.nvme_smart.has_value())
            continue;

        const NvmeSmart::HealthInfo& info = item.nvme_smart.value();
        proto::system_info::PhysicalDrives::Drive::NvmeHealth* health =
            drive->mutable_nvme_health();

        health->set_critical_warning(info.critical_warning);
        health->set_composite_temperature(info.composite_temperature);

        // Sent in full so that the position of a sensor stays its number.
        for (int i = 0; i < NvmeSmart::kTemperatureSensorCount; ++i)
            health->add_temperature_sensor(info.temperature_sensor[i]);

        health->set_available_spare(info.available_spare);
        health->set_available_spare_threshold(info.available_spare_threshold);
        health->set_percentage_used(info.percentage_used);
        health->set_data_units_read(info.data_units_read);
        health->set_data_units_written(info.data_units_written);
        health->set_host_read_commands(info.host_read_commands);
        health->set_host_write_commands(info.host_write_commands);
        health->set_controller_busy_time(info.controller_busy_time);
        health->set_power_cycles(info.power_cycles);
        health->set_power_on_hours(info.power_on_hours);
        health->set_unsafe_shutdowns(info.unsafe_shutdowns);
        health->set_media_errors(info.media_errors);
        health->set_error_log_entries(info.error_log_entries);
        health->set_warning_temperature_time(info.warning_temperature_time);
        health->set_critical_temperature_time(info.critical_temperature_time);
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
    fillDmi(system_info);
    fillDrives(system_info);
}

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

    system_info->mutable_header()->set_category(category);

    if (category == kSystemInfo_Summary)
    {
        fillSummaryInfo(system_info);
    }
    else if (category == kSystemInfo_Devices)
    {
        fillDevices(system_info);
    }
    else if (category == kSystemInfo_Drives)
    {
        fillPhysicalDrives(system_info);
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
    else if (category == kSystemInfo_Dmi)
    {
        fillDmi(system_info);
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

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWorker::SysInfoWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
SysInfoWorker::~SysInfoWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
quint32 SysInfoWorker::createConsumerId()
{
    static std::atomic<quint32> last_id { 0 };
    return ++last_id;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::onQuery(quint32 consumer_id, const QByteArray& buffer)
{
    proto::system_info::SystemInfoRequest request;
    if (!parse(buffer, &request))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(request, &system_info);

    emit sig_systemInfo(consumer_id, serialize(system_info));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::onStart()
{
    LOG(INFO) << "Sys info worker started";
}

//--------------------------------------------------------------------------------------------------
void SysInfoWorker::onStop()
{
    LOG(INFO) << "Sys info worker stopped";
}
