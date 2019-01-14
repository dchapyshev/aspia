//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_system_info.h"

#include "base/win/drive_enumerator.h"
#include "base/win/printer_enumerator.h"
#include "base/smbios_parser.h"
#include "base/smbios_reader.h"
#include "base/sys_info.h"
#include "network/network_adapter_enumerator.h"

namespace aspia {

void createHostSystemInfo(proto::system_info::SystemInfo* system_info)
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

    proto::system_info::Processor* processor = system_info->mutable_processor();
    processor->set_vendor(base::SysInfo::processorVendor());
    processor->set_model(base::SysInfo::processorName());
    processor->set_packages(base::SysInfo::processorPackages());
    processor->set_cores(base::SysInfo::processorCores());
    processor->set_threads(base::SysInfo::processorThreads());

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

    for (base::win::PrinterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::Printers::Printer* printer =
            system_info->mutable_printers()->add_printer();

        printer->set_name(enumerator.name());
        printer->set_default_(enumerator.isDefault());
        printer->set_shared(enumerator.isShared());
        printer->set_port(enumerator.portName());
        printer->set_driver(enumerator.driverName());
        printer->set_jobs_count(enumerator.jobsCount());
        printer->set_share_name(enumerator.shareName());
    }

    for (NetworkAdapterEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::system_info::NetworkAdapters::Adapter* adapter =
            system_info->mutable_network_adapters()->add_adapter();

        adapter->set_adapter_name(enumerator.adapterName());
        adapter->set_connection_name(enumerator.connectionName());
        adapter->set_interface(enumerator.interfaceType());
        adapter->set_speed(enumerator.speed());
        adapter->set_mac(enumerator.macAddress());
        adapter->set_dhcp_enabled(enumerator.isDhcpEnabled());

        for (NetworkAdapterEnumerator::GatewayEnumerator gateway(enumerator);
             !gateway.isAtEnd(); gateway.advance())
        {
            adapter->add_gateway()->assign(gateway.address());
        }

        for (NetworkAdapterEnumerator::IpAddressEnumerator ip(enumerator);
             !ip.isAtEnd(); ip.advance())
        {
            proto::system_info::NetworkAdapters::Adapter::Address* address = adapter->add_address();

            address->set_ip(ip.address());
            address->set_mask(ip.mask());
        }

        for (NetworkAdapterEnumerator::DnsEnumerator dns(enumerator);
             !dns.isAtEnd(); dns.advance())
        {
            adapter->add_dns()->assign(dns.address());
        }

        for (NetworkAdapterEnumerator::DhcpEnumerator dhcp(enumerator);
             !dhcp.isAtEnd(); dhcp.advance())
        {
            adapter->add_dhcp()->append(dhcp.address());
        }
    }
}

} // namespace aspia
