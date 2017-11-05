//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/devices/monitor_enumerator.h"
#include "base/devices/smbios_reader.h"
#include "base/strings/string_util.h"
#include "protocol/category_group_hardware.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryDmiBios
//

const char* CategoryDmiBios::Name() const
{
    return "BIOS";
}

Category::IconId CategoryDmiBios::Icon() const
{
    return IDI_BIOS;
}

const char* CategoryDmiBios::Guid() const
{
    return "B0B73D57-2CDC-4814-9AE0-C7AF7DDDD60E";
}

void CategoryDmiBios::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::DmiBios message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    if (!message.manufacturer().empty())
        output->AddParam(IDI_BIOS, "Manufacturer", message.manufacturer());

    if (!message.version().empty())
        output->AddParam(IDI_BIOS, "Version", message.version());

    if (!message.date().empty())
        output->AddParam(IDI_BIOS, "Date", message.date());

    if (message.size() != 0)
        output->AddParam(IDI_BIOS, "Size", std::to_string(message.size()), "kB");

    if (!message.bios_revision().empty())
        output->AddParam(IDI_BIOS, "BIOS Revision", message.bios_revision());

    if (!message.firmware_revision().empty())
        output->AddParam(IDI_BIOS, "Firmware Revision", message.firmware_revision());

    if (!message.address().empty())
        output->AddParam(IDI_BIOS, "Address", message.address());

    if (message.runtime_size() != 0)
        output->AddParam(IDI_BIOS, "Runtime Size", std::to_string(message.runtime_size()), "Bytes");

    if (message.feature_size() > 0)
    {
        Output::Group group(output, "Supported Features", IDI_BIOS);

        for (int index = 0; index < message.feature_size(); ++index)
        {
            const system_info::DmiBios::Feature& feature = message.feature(index);

            output->AddParam(feature.supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             feature.name(),
                             feature.supported() ? "Yes" : "No");
        }
    }
}

std::string CategoryDmiBios::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumerator<SMBios::BiosTable> table_enumerator(*smbios);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::BiosTable table = table_enumerator.GetTable();
    system_info::DmiBios message;

    message.set_manufacturer(table.GetManufacturer());
    message.set_version(table.GetVersion());
    message.set_date(table.GetDate());
    message.set_size(table.GetSize());
    message.set_bios_revision(table.GetBiosRevision());
    message.set_firmware_revision(table.GetFirmwareRevision());
    message.set_address(table.GetAddress());
    message.set_runtime_size(table.GetRuntimeSize());

    SMBios::BiosTable::FeatureList feature_list = table.GetCharacteristics();

    for (const auto& feature : feature_list)
    {
        system_info::DmiBios::Feature* item = message.add_feature();
        item->set_name(feature.first);
        item->set_supported(feature.second);
    }

    return message.SerializeAsString();
}

//
// CategoryDmiSystem
//

const char* CategoryDmiSystem::Name() const
{
    return "System";
}

Category::IconId CategoryDmiSystem::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategoryDmiSystem::Guid() const
{
    return "F599BBA4-AEBB-4583-A15E-9848F4C98601";
}

void CategoryDmiSystem::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::DmiSystem message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    if (!message.manufacturer().empty())
        output->AddParam(IDI_COMPUTER, "Manufacturer", message.manufacturer());

    if (!message.product_name().empty())
        output->AddParam(IDI_COMPUTER, "Product Name", message.product_name());

    if (!message.version().empty())
        output->AddParam(IDI_COMPUTER, "Version", message.version());

    if (!message.serial_number().empty())
        output->AddParam(IDI_COMPUTER, "Serial Number", message.serial_number());

    if (!message.uuid().empty())
        output->AddParam(IDI_COMPUTER, "UUID", message.uuid());

    if (!message.wakeup_type().empty())
        output->AddParam(IDI_COMPUTER, "Wakeup Type", message.wakeup_type());

    if (!message.sku_number().empty())
        output->AddParam(IDI_COMPUTER, "SKU Number", message.sku_number());

    if (!message.family().empty())
        output->AddParam(IDI_COMPUTER, "Family", message.family());
}

std::string CategoryDmiSystem::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumerator<SMBios::SystemTable> table_enumerator(*smbios);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::SystemTable table = table_enumerator.GetTable();
    system_info::DmiSystem message;

    message.set_manufacturer(table.GetManufacturer());
    message.set_product_name(table.GetProductName());
    message.set_version(table.GetVersion());
    message.set_serial_number(table.GetSerialNumber());
    message.set_uuid(table.GetUUID());
    message.set_wakeup_type(table.GetWakeupType());
    message.set_sku_number(table.GetSKUNumber());
    message.set_family(table.GetFamily());

    return message.SerializeAsString();
}

//
// CategoryDmiMotherboard
//

const char* CategoryDmiBaseboard::Name() const
{
    return "Baseboard";
}

Category::IconId CategoryDmiBaseboard::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiBaseboard::Guid() const
{
    return "8143642D-3248-40F5-8FCF-629C581FFF01";
}

void CategoryDmiBaseboard::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::DmiBaseboard message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::DmiBaseboard::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Baseboard #%d", index + 1), Icon());

        const char* type;
        switch (item.type())
        {
            case system_info::DmiBaseboard::Item::BOARD_TYPE_OTHER:
                type = "Other";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_SERVER_BLADE:
                type =  "Server Blade";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_CONNECTIVITY_SWITCH:
                type = "Connectivity Switch";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_SYSTEM_MANAGEMENT_MODULE:
                type = "System Management Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_MODULE:
                type = "Processor Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_IO_MODULE:
                type = "I/O Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_MEMORY_MODULE:
                type = "Memory Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_DAUGHTER_BOARD:
                type = "Daughter Board";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_MOTHERBOARD:
                type = "Motherboard";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_PLUS_MEMORY_MODULE:
                type = "Processor + Memory Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_PLUS_IO_MODULE:
                type = "Processor + I/O Module";
                break;

            case system_info::DmiBaseboard::Item::BOARD_TYPE_INTERCONNECT_BOARD:
                type = "Interconnect Board";
                break;

            default:
                type = nullptr;
                break;
        }

        if (type != nullptr)
            output->AddParam(IDI_MOTHERBOARD, "Type", type);

        if (!item.manufacturer().empty())
            output->AddParam(IDI_MOTHERBOARD, "Manufacturer", item.manufacturer());

        if (!item.product_name().empty())
            output->AddParam(IDI_MOTHERBOARD, "Product Name", item.product_name());

        if (!item.version().empty())
            output->AddParam(IDI_MOTHERBOARD, "Version", item.version());

        if (!item.serial_number().empty())
            output->AddParam(IDI_MOTHERBOARD, "Serial Number", item.serial_number());

        if (!item.asset_tag().empty())
            output->AddParam(IDI_MOTHERBOARD, "Asset Tag", item.asset_tag());

        if (item.feature_size() > 0)
        {
            Output::Group features_group(output, "Supported Features", IDI_MOTHERBOARD);

            for (int i = 0; i < item.feature_size(); ++i)
            {
                const system_info::DmiBaseboard::Item::Feature& feature = item.feature(i);

                output->AddParam(feature.supported() ? IDI_CHECKED : IDI_UNCHECKED,
                                 feature.name(),
                                 feature.supported() ? "Yes" : "No");
            }
        }

        if (!item.location_in_chassis().empty())
            output->AddParam(IDI_MOTHERBOARD, "Location in chassis", item.location_in_chassis());
    }
}

std::string CategoryDmiBaseboard::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiBaseboard message;

    for (SMBios::TableEnumerator<SMBios::BaseboardTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::BaseboardTable table = table_enumerator.GetTable();
        system_info::DmiBaseboard::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_product_name(table.GetProductName());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_location_in_chassis(table.GetLocationInChassis());

        switch (table.GetBoardType())
        {
            case SMBios::BaseboardTable::BoardType::OTHER:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_OTHER);
                break;

            case SMBios::BaseboardTable::BoardType::SERVER_BLADE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_SERVER_BLADE);
                break;

            case SMBios::BaseboardTable::BoardType::CONNECTIVITY_SWITCH:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_CONNECTIVITY_SWITCH);
                break;

            case SMBios::BaseboardTable::BoardType::SYSTEM_MANAGEMENT_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_SYSTEM_MANAGEMENT_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::PROCESSOR_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::IO_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_IO_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::MEMORY_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_MEMORY_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::DAUGHTER_BOARD:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_DAUGHTER_BOARD);
                break;

            case SMBios::BaseboardTable::BoardType::MOTHERBOARD:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_MOTHERBOARD);
                break;

            case SMBios::BaseboardTable::BoardType::PROCESSOR_PLUS_MEMORY_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_PLUS_MEMORY_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::PROCESSOR_PLUS_IO_MODULE:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_PROCESSOR_PLUS_IO_MODULE);
                break;

            case SMBios::BaseboardTable::BoardType::INTERCONNECT_BOARD:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_INTERCONNECT_BOARD);
                break;

            default:
                item->set_type(system_info::DmiBaseboard::Item::BOARD_TYPE_UNKNOWN);
                break;
        }

        SMBios::BaseboardTable::FeatureList feature_list = table.GetFeatures();

        for (const auto& feature : feature_list)
        {
            system_info::DmiBaseboard::Item::Feature* feature_item = item->add_feature();
            feature_item->set_name(feature.first);
            feature_item->set_supported(feature.second);
        }
    }

    return message.SerializeAsString();
}

//
// CategoryDmiChassis
//

const char* CategoryDmiChassis::Name() const
{
    return "Chassis";
}

Category::IconId CategoryDmiChassis::Icon() const
{
    return IDI_SERVER;
}

const char* CategoryDmiChassis::Guid() const
{
    return "81D9E51F-4A86-49FC-A37F-232D6A62EC45";
}

void CategoryDmiChassis::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::DmiChassis message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::DmiChassis::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Chassis #%d", index + 1), Icon());

        if (!item.manufacturer().empty())
            output->AddParam(IDI_SERVER, "Manufacturer", item.manufacturer());

        if (!item.version().empty())
            output->AddParam(IDI_SERVER, "Version", item.version());

        if (!item.serial_number().empty())
            output->AddParam(IDI_SERVER, "Serial Number", item.serial_number());

        if (!item.asset_tag().empty())
            output->AddParam(IDI_SERVER, "Asset Tag", item.asset_tag());

        const char* type;
        switch (item.type())
        {
            case system_info::DmiChassis::Item::TYPE_OTHER:
                type = "Other";
                break;

            case system_info::DmiChassis::Item::TYPE_DESKTOP:
                type = "Desktop";
                break;

            case system_info::DmiChassis::Item::TYPE_LOW_PROFILE_DESKTOP:
                type = "Low Profile Desktop";
                break;

            case system_info::DmiChassis::Item::TYPE_PIZZA_BOX:
                type = "Pizza Box";
                break;

            case system_info::DmiChassis::Item::TYPE_MINI_TOWER:
                type = "Mini Tower";
                break;

            case system_info::DmiChassis::Item::TYPE_TOWER:
                type = "Tower";
                break;

            case system_info::DmiChassis::Item::TYPE_PORTABLE:
                type = "Portable";
                break;

            case system_info::DmiChassis::Item::TYPE_LAPTOP:
                type = "Laptop";
                break;

            case system_info::DmiChassis::Item::TYPE_NOTEBOOK:
                type = "Notebook";
                break;

            case system_info::DmiChassis::Item::TYPE_HAND_HELD:
                type = "Hand Held";
                break;

            case system_info::DmiChassis::Item::TYPE_DOCKING_STATION:
                type = "Docking Station";
                break;

            case system_info::DmiChassis::Item::TYPE_ALL_IN_ONE:
                type = "All In One";
                break;

            case system_info::DmiChassis::Item::TYPE_SUB_NOTEBOOK:
                type = "Sub Notebook";
                break;

            case system_info::DmiChassis::Item::TYPE_SPACE_SAVING:
                type = "Space Saving";
                break;

            case system_info::DmiChassis::Item::TYPE_LUNCH_BOX:
                type = "Lunch Box";
                break;

            case system_info::DmiChassis::Item::TYPE_MAIN_SERVER_CHASSIS:
                type = "Main Server Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_EXPANSION_CHASSIS:
                type = "Expansion Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_SUB_CHASSIS:
                type = "Sub Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_BUS_EXPANSION_CHASSIS:
                type = "Bus Expansion Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_PERIPHERIAL_CHASSIS:
                type = "Peripherial Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_RAID_CHASSIS:
                type = "RAID Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_RACK_MOUNT_CHASSIS:
                type = "Rack Mount Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_SEALED_CASE_PC:
                type = "Sealed Case PC";
                break;

            case system_info::DmiChassis::Item::TYPE_MULTI_SYSTEM_CHASSIS:
                type = "Multi System Chassis";
                break;

            case system_info::DmiChassis::Item::TYPE_COMPACT_PCI:
                type = "Compact PCI";
                break;

            case system_info::DmiChassis::Item::TYPE_ADVANCED_TCA:
                type = "Advanced TCA";
                break;

            case system_info::DmiChassis::Item::TYPE_BLADE:
                type = "Blade";
                break;

            case system_info::DmiChassis::Item::TYPE_BLADE_ENCLOSURE:
                type = "Blade Enclosure";
                break;

            default:
                type = nullptr;
                break;
        }

        if (type != nullptr)
            output->AddParam(IDI_SERVER, "Type", type);

        auto status_to_string = [](system_info::DmiChassis::Item::Status status)
        {
            switch (status)
            {
                case system_info::DmiChassis::Item::STATUS_OTHER:
                    return "Other";

                case system_info::DmiChassis::Item::STATUS_SAFE:
                    return "Safe";

                case system_info::DmiChassis::Item::STATUS_WARNING:
                    return "Warning";

                case system_info::DmiChassis::Item::STATUS_CRITICAL:
                    return "Critical";

                case system_info::DmiChassis::Item::STATUS_NON_RECOVERABLE:
                    return "Non Recoverable";

                default:
                    return "Unknown";
            }
        };

        output->AddParam(IDI_SERVER, "OS Load Status", status_to_string(item.os_load_status()));
        output->AddParam(IDI_SERVER, "Power Source Status", status_to_string(item.power_source_status()));
        output->AddParam(IDI_SERVER, "Temperature Status", status_to_string(item.temparature_status()));

        const char* status;
        switch (item.security_status())
        {
            case system_info::DmiChassis::Item::SECURITY_STATUS_OTHER:
                status = "Other";
                break;

            case system_info::DmiChassis::Item::SECURITY_STATUS_NONE:
                status = "None";
                break;

            case system_info::DmiChassis::Item::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT:
                status = "External Interface Locked Out";
                break;

            case system_info::DmiChassis::Item::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED:
                status = "External Interface Enabled";
                break;

            default:
                status = nullptr;
                break;
        }

        if (status != nullptr)
            output->AddParam(IDI_SERVER, "Security Status", status);

        if (item.height() != 0)
            output->AddParam(IDI_SERVER, "Height", std::to_string(item.height()), "U");

        if (item.number_of_power_cords() != 0)
        {
            output->AddParam(IDI_SERVER,
                             "Number Of Power Cords",
                             std::to_string(item.number_of_power_cords()));
        }
    }
}

std::string CategoryDmiChassis::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiChassis message;

    for (SMBios::TableEnumerator<SMBios::ChassisTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ChassisTable table = table_enumerator.GetTable();
        system_info::DmiChassis::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());

        switch (table.GetType())
        {
            case SMBios::ChassisTable::Type::OTHER:
                item->set_type(system_info::DmiChassis::Item::TYPE_OTHER);
                break;

            case SMBios::ChassisTable::Type::DESKTOP:
                item->set_type(system_info::DmiChassis::Item::TYPE_DESKTOP);
                break;

            case SMBios::ChassisTable::Type::LOW_PROFILE_DESKTOP:
                item->set_type(system_info::DmiChassis::Item::TYPE_LOW_PROFILE_DESKTOP);
                break;

            case SMBios::ChassisTable::Type::PIZZA_BOX:
                item->set_type(system_info::DmiChassis::Item::TYPE_PIZZA_BOX);
                break;

            case SMBios::ChassisTable::Type::MINI_TOWER:
                item->set_type(system_info::DmiChassis::Item::TYPE_MINI_TOWER);
                break;

            case SMBios::ChassisTable::Type::TOWER:
                item->set_type(system_info::DmiChassis::Item::TYPE_TOWER);
                break;

            case SMBios::ChassisTable::Type::PORTABLE:
                item->set_type(system_info::DmiChassis::Item::TYPE_PORTABLE);
                break;

            case SMBios::ChassisTable::Type::LAPTOP:
                item->set_type(system_info::DmiChassis::Item::TYPE_LAPTOP);
                break;

            case SMBios::ChassisTable::Type::NOTEBOOK:
                item->set_type(system_info::DmiChassis::Item::TYPE_NOTEBOOK);
                break;

            case SMBios::ChassisTable::Type::HAND_HELD:
                item->set_type(system_info::DmiChassis::Item::TYPE_HAND_HELD);
                break;

            case SMBios::ChassisTable::Type::DOCKING_STATION:
                item->set_type(system_info::DmiChassis::Item::TYPE_DOCKING_STATION);
                break;

            case SMBios::ChassisTable::Type::ALL_IN_ONE:
                item->set_type(system_info::DmiChassis::Item::TYPE_ALL_IN_ONE);
                break;

            case SMBios::ChassisTable::Type::SUB_NOTEBOOK:
                item->set_type(system_info::DmiChassis::Item::TYPE_SUB_NOTEBOOK);
                break;

            case SMBios::ChassisTable::Type::SPACE_SAVING:
                item->set_type(system_info::DmiChassis::Item::TYPE_SPACE_SAVING);
                break;

            case SMBios::ChassisTable::Type::LUNCH_BOX:
                item->set_type(system_info::DmiChassis::Item::TYPE_LUNCH_BOX);
                break;

            case SMBios::ChassisTable::Type::MAIN_SERVER_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_MAIN_SERVER_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::EXPANSION_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_EXPANSION_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::SUB_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_SUB_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::BUS_EXPANSION_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_BUS_EXPANSION_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::PERIPHERIAL_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_PERIPHERIAL_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::RAID_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_RAID_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::RACK_MOUNT_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_RACK_MOUNT_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::SEALED_CASE_PC:
                item->set_type(system_info::DmiChassis::Item::TYPE_SEALED_CASE_PC);
                break;

            case SMBios::ChassisTable::Type::MULTI_SYSTEM_CHASSIS:
                item->set_type(system_info::DmiChassis::Item::TYPE_MULTI_SYSTEM_CHASSIS);
                break;

            case SMBios::ChassisTable::Type::COMPACT_PCI:
                item->set_type(system_info::DmiChassis::Item::TYPE_COMPACT_PCI);
                break;

            case SMBios::ChassisTable::Type::ADVANCED_TCA:
                item->set_type(system_info::DmiChassis::Item::TYPE_ADVANCED_TCA);
                break;

            case SMBios::ChassisTable::Type::BLADE:
                item->set_type(system_info::DmiChassis::Item::TYPE_BLADE);
                break;

            case SMBios::ChassisTable::Type::BLADE_ENCLOSURE:
                item->set_type(system_info::DmiChassis::Item::TYPE_BLADE_ENCLOSURE);
                break;

            default:
                item->set_type(system_info::DmiChassis::Item::TYPE_UNKNOWN);
                break;
        }

        auto status_to_proto_status = [](SMBios::ChassisTable::Status status)
        {
            switch (status)
            {
                case SMBios::ChassisTable::Status::OTHER:
                    return system_info::DmiChassis::Item::STATUS_OTHER;

                case SMBios::ChassisTable::Status::SAFE:
                    return system_info::DmiChassis::Item::STATUS_SAFE;

                case SMBios::ChassisTable::Status::WARNING:
                    return system_info::DmiChassis::Item::STATUS_WARNING;

                case SMBios::ChassisTable::Status::CRITICAL:
                    return system_info::DmiChassis::Item::STATUS_CRITICAL;

                case SMBios::ChassisTable::Status::NON_RECOVERABLE:
                    return system_info::DmiChassis::Item::STATUS_NON_RECOVERABLE;

                default:
                    return system_info::DmiChassis::Item::STATUS_UNKNOWN;
            }
        };

        item->set_os_load_status(status_to_proto_status(table.GetOSLoadStatus()));
        item->set_power_source_status(status_to_proto_status(table.GetPowerSourceStatus()));
        item->set_temparature_status(status_to_proto_status(table.GetTemperatureStatus()));

        switch (table.GetSecurityStatus())
        {
            case SMBios::ChassisTable::SecurityStatus::OTHER:
                item->set_security_status(system_info::DmiChassis::Item::SECURITY_STATUS_OTHER);
                break;

            case SMBios::ChassisTable::SecurityStatus::NONE:
                item->set_security_status(system_info::DmiChassis::Item::SECURITY_STATUS_NONE);
                break;

            case SMBios::ChassisTable::SecurityStatus::EXTERNAL_INTERFACE_LOCKED_OUT:
                item->set_security_status(
                    system_info::DmiChassis::Item::SECURITY_STATUS_EXTERNAL_INTERFACE_LOCKED_OUT);
                break;

            case SMBios::ChassisTable::SecurityStatus::EXTERNAL_INTERFACE_ENABLED:
                item->set_security_status(
                    system_info::DmiChassis::Item::SECURITY_STATUS_EXTERNAL_INTERFACE_ENABLED);
                break;

            default:
                item->set_security_status(system_info::DmiChassis::Item::SECURITY_STATUS_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

//
// CategoryDmiCaches
//

const char* CategoryDmiCaches::Name() const
{
    return "Caches";
}

Category::IconId CategoryDmiCaches::Icon() const
{
    return IDI_CHIP;
}

const char* CategoryDmiCaches::Guid() const
{
    return "BA9258E7-0046-4A77-A97B-0407453706A3";
}

void CategoryDmiCaches::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiCaches::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiProcessors
//

const char* CategoryDmiProcessors::Name() const
{
    return "Processors";
}

Category::IconId CategoryDmiProcessors::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryDmiProcessors::Guid() const
{
    return "84D8B0C3-37A4-4825-A523-40B62E0CADC3";
}

void CategoryDmiProcessors::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiProcessors::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiMemoryDevices
//

const char* CategoryDmiMemoryDevices::Name() const
{
    return "Memory Devices";
}

Category::IconId CategoryDmiMemoryDevices::Icon() const
{
    return IDI_MEMORY;
}

const char* CategoryDmiMemoryDevices::Guid() const
{
    return "9C591459-A83F-4F48-883D-927765C072B0";
}

void CategoryDmiMemoryDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiMemoryDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiSystemSlots
//

const char* CategoryDmiSystemSlots::Name() const
{
    return "System Slots";
}

Category::IconId CategoryDmiSystemSlots::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiSystemSlots::Guid() const
{
    return "7A4F71C6-557F-48A5-AC94-E430F69154F1";
}

void CategoryDmiSystemSlots::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiSystemSlots::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiPortConnectors
//

const char* CategoryDmiPortConnectors::Name() const
{
    return "Port Connectors";
}

Category::IconId CategoryDmiPortConnectors::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiPortConnectors::Guid() const
{
    return "FF4CE0FE-261F-46EF-852F-42420E68CFD2";
}

void CategoryDmiPortConnectors::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiPortConnectors::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiOnboardDevices
//

const char* CategoryDmiOnboardDevices::Name() const
{
    return "Onboard Devices";
}

Category::IconId CategoryDmiOnboardDevices::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiOnboardDevices::Guid() const
{
    return "6C62195C-5E5F-41BA-B6AD-99041594DAC6";
}

void CategoryDmiOnboardDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiOnboardDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiBuildinPointing
//

const char* CategoryDmiBuildinPointing::Name() const
{
    return "Build-in Pointing";
}

Category::IconId CategoryDmiBuildinPointing::Icon() const
{
    return IDI_MOUSE;
}

const char* CategoryDmiBuildinPointing::Guid() const
{
    return "6883684B-3CEC-451B-A2E3-34C16348BA1B";
}

void CategoryDmiBuildinPointing::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiBuildinPointing::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiPortableBattery
//

const char* CategoryDmiPortableBattery::Name() const
{
    return "Portable Battery";
}

Category::IconId CategoryDmiPortableBattery::Icon() const
{
    return IDI_BATTERY;
}

const char* CategoryDmiPortableBattery::Guid() const
{
    return "0CA213B5-12EE-4828-A399-BA65244E65FD";
}

void CategoryDmiPortableBattery::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiPortableBattery::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupDMI
//

const char* CategoryGroupDMI::Name() const
{
    return "DMI";
}

Category::IconId CategoryGroupDMI::Icon() const
{
    return IDI_COMPUTER;
}

//
// CategoryCPU
//

const char* CategoryCPU::Name() const
{
    return "Central Processor";
}

Category::IconId CategoryCPU::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryCPU::Guid() const
{
    return "31D1312E-85A9-419A-91B4-BA81129B3CCC";
}

void CategoryCPU::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryCPU::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryOpticalDrives
//

const char* CategoryOpticalDrives::Name() const
{
    return "Optical Drives";
}

Category::IconId CategoryOpticalDrives::Icon() const
{
    return IDI_DRIVE_DISK;
}

const char* CategoryOpticalDrives::Guid() const
{
    return "68E028FE-3DA6-4BAF-9E18-CDB828372860";
}

void CategoryOpticalDrives::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryOpticalDrives::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryATA
//

const char* CategoryATA::Name() const
{
    return "ATA";
}

Category::IconId CategoryATA::Icon() const
{
    return IDI_DRIVE;
}

const char* CategoryATA::Guid() const
{
    return "79D80586-D264-46E6-8718-09E267730B78";
}

void CategoryATA::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryATA::Serialize()
{
    // TODO
    return std::string();
}

//
// CategorySMART
//

const char* CategorySMART::Name() const
{
    return "S.M.A.R.T.";
}

Category::IconId CategorySMART::Icon() const
{
    return IDI_DRIVE;
}

const char* CategorySMART::Guid() const
{
    return "7B1F2ED7-7A2E-4F5C-A70B-A56AB5B8CE00";
}

void CategorySMART::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategorySMART::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupStorage
//

const char* CategoryGroupStorage::Name() const
{
    return "Storage";
}

Category::IconId CategoryGroupStorage::Icon() const
{
    return IDI_DRIVE;
}

//
// CategoryWindowsVideo
//

const char* CategoryWindowsVideo::Name() const
{
    return "Windows Video";
}

Category::IconId CategoryWindowsVideo::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryWindowsVideo::Guid() const
{
    return "09E9069D-C394-4CD7-8252-E5CF83B7674C";
}

void CategoryWindowsVideo::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryWindowsVideo::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryMonitor
//

const char* CategoryMonitor::Name() const
{
    return "Monitor";
}

Category::IconId CategoryMonitor::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryMonitor::Guid() const
{
    return "281100E4-88ED-4AE2-BC4A-3A37282BBAB5";
}

void CategoryMonitor::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::Monitors message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::Monitors::Item& item = message.item(index);

        Output::Group group(output, item.system_name(), Icon());

        if (!item.monitor_name().empty())
            output->AddParam(IDI_MONITOR, "Monitor Name", item.monitor_name());

        if (!item.manufacturer_name().empty())
            output->AddParam(IDI_MONITOR, "Manufacturer Name", item.manufacturer_name());

        if (!item.monitor_id().empty())
            output->AddParam(IDI_MONITOR, "Monitor ID", item.monitor_id());

        if (!item.serial_number().empty())
            output->AddParam(IDI_MONITOR, "Serial Number", item.serial_number());

        if (item.edid_version() != 0)
            output->AddParam(IDI_MONITOR, "EDID Version", std::to_string(item.edid_version()));

        if (item.edid_revision() != 0)
            output->AddParam(IDI_MONITOR, "EDID Revision", std::to_string(item.edid_revision()));

        if (item.week_of_manufacture() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Week Of Manufacture",
                             std::to_string(item.week_of_manufacture()));
        }

        if (item.year_of_manufacture() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Year Of Manufacture",
                             std::to_string(item.year_of_manufacture()));
        }

        if (item.gamma() != 0.0)
            output->AddParam(IDI_MONITOR, "Gamma", StringPrintf("%.2f", item.gamma()));

        if (item.max_horizontal_image_size() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Horizontal Image Size",
                             std::to_string(item.max_horizontal_image_size()),
                             "cm");
        }

        if (item.max_vertical_image_size() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Vertical Image Size",
                             std::to_string(item.max_vertical_image_size()),
                             "cm");
        }
        if (item.max_horizontal_image_size() != 0 && item.max_vertical_image_size() != 0)
        {
            // Calculate the monitor diagonal by the Pythagorean theorem and translate from
            // centimeters to inches.
            double diagonal_size =
                sqrt((item.max_horizontal_image_size() * item.max_horizontal_image_size()) +
                (item.max_vertical_image_size() * item.max_vertical_image_size())) / 2.54;

            output->AddParam(IDI_MONITOR,
                             "Diagonal Size",
                             StringPrintf("%.0f", diagonal_size),
                             "\"");
        }

        if (item.horizontal_resolution() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Horizontal Resolution",
                             std::to_string(item.horizontal_resolution()),
                             "px");
        }

        if (item.vertical_resoulution() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Vertical Resolution",
                             std::to_string(item.vertical_resoulution()),
                             "px");
        }

        if (item.min_horizontal_rate() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Minimum Horizontal Frequency",
                             std::to_string(item.min_horizontal_rate()),
                             "kHz");
        }

        if (item.max_horizontal_rate() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Maximum Horizontal Frequency",
                             std::to_string(item.max_horizontal_rate()),
                             "kHz");
        }

        if (item.min_vertical_rate() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Minimum Vertical Frequency",
                             std::to_string(item.min_vertical_rate()),
                             "Hz");
        }

        if (item.max_vertical_rate() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Maximum Vertical Frequency",
                             std::to_string(item.max_vertical_rate()),
                             "Hz");
        }

        if (item.pixel_clock() != 0.0)
        {
            output->AddParam(IDI_MONITOR,
                             "Pixel Clock",
                             StringPrintf("%.2f", item.pixel_clock()),
                             "MHz");
        }

        if (item.max_pixel_clock() != 0)
        {
            output->AddParam(IDI_MONITOR,
                             "Maximum Pixel Clock",
                             std::to_string(item.max_pixel_clock()),
                             "MHz");
        }

        switch (item.input_signal_type())
        {
            case system_info::Monitors::Item::INPUT_SIGNAL_TYPE_ANALOG:
                output->AddParam(IDI_MONITOR, "Input Signal Type", "Analog");
                break;

            case system_info::Monitors::Item::INPUT_SIGNAL_TYPE_DIGITAL:
                output->AddParam(IDI_MONITOR, "Input Signal Type", "Digital");
                break;

            default:
                break;
        }

        {
            Output::Group features_group(output, "Supported Features", Icon());

            output->AddParam(item.default_gtf_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "Default GTF",
                             item.default_gtf_supported() ? "Yes" : "No");

            output->AddParam(item.suspend_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "Suspend",
                             item.suspend_supported() ? "Yes" : "No");

            output->AddParam(item.standby_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "Standby",
                             item.standby_supported() ? "Yes" : "No");

            output->AddParam(item.active_off_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "Active Off",
                             item.active_off_supported() ? "Yes" : "No");

            output->AddParam(item.preferred_timing_mode_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "Preferred Timing Mode",
                             item.preferred_timing_mode_supported() ? "Yes" : "No");

            output->AddParam(item.srgb_supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             "sRGB",
                             item.srgb_supported() ? "Yes" : "No");
        }

        if (item.timings_size() > 0)
        {
            Output::Group features_group(output, "Supported Video Modes", Icon());

            for (int mode = 0; mode < item.timings_size(); ++mode)
            {
                const system_info::Monitors::Item::Timing& timing = item.timings(mode);

                output->AddParam(IDI_CHECKED,
                                 StringPrintf("%dx%d", timing.width(), timing.height()),
                                 std::to_string(timing.frequency()),
                                 "Hz");
            }
        }
    }
}

std::string CategoryMonitor::Serialize()
{
    system_info::Monitors message;

    for (MonitorEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        std::unique_ptr<EdidParser> edid = enumerator.GetEDID();
        if (!edid)
            continue;

        system_info::Monitors::Item* item = message.add_item();

        std::string system_name = enumerator.GetFriendlyName();

        if (system_name.empty())
            system_name = enumerator.GetDescription();

        item->set_system_name(system_name);
        item->set_monitor_name(edid->GetMonitorName());
        item->set_manufacturer_name(edid->GetManufacturerName());
        item->set_monitor_id(edid->GetMonitorId());
        item->set_serial_number(edid->GetSerialNumber());
        item->set_edid_version(edid->GetEdidVersion());
        item->set_edid_revision(edid->GetEdidRevision());
        item->set_week_of_manufacture(edid->GetWeekOfManufacture());
        item->set_year_of_manufacture(edid->GetYearOfManufacture());
        item->set_max_horizontal_image_size(edid->GetMaxHorizontalImageSize());
        item->set_max_vertical_image_size(edid->GetMaxVerticalImageSize());
        item->set_horizontal_resolution(edid->GetHorizontalResolution());
        item->set_vertical_resoulution(edid->GetVerticalResolution());
        item->set_gamma(edid->GetGamma());
        item->set_max_horizontal_rate(edid->GetMaxHorizontalRate());
        item->set_min_horizontal_rate(edid->GetMinHorizontalRate());
        item->set_max_vertical_rate(edid->GetMaxVerticalRate());
        item->set_min_vertical_rate(edid->GetMinVerticalRate());
        item->set_pixel_clock(edid->GetPixelClock());
        item->set_max_pixel_clock(edid->GetMaxSupportedPixelClock());

        switch (edid->GetInputSignalType())
        {
            case EdidParser::INPUT_SIGNAL_TYPE_DIGITAL:
                item->set_input_signal_type(system_info::Monitors::Item::INPUT_SIGNAL_TYPE_DIGITAL);
                break;

            default:
                item->set_input_signal_type(system_info::Monitors::Item::INPUT_SIGNAL_TYPE_ANALOG);
                break;
        }

        uint8_t supported_features = edid->GetFeatureSupport();

        if (supported_features & EdidParser::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            item->set_default_gtf_supported(true);

        if (supported_features & EdidParser::FEATURE_SUPPORT_SUSPEND)
            item->set_suspend_supported(true);

        if (supported_features & EdidParser::FEATURE_SUPPORT_STANDBY)
            item->set_standby_supported(true);

        if (supported_features & EdidParser::FEATURE_SUPPORT_ACTIVE_OFF)
            item->set_active_off_supported(true);

        if (supported_features & EdidParser::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            item->set_preferred_timing_mode_supported(true);

        if (supported_features & EdidParser::FEATURE_SUPPORT_SRGB)
            item->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            system_info::Monitors::Item::Timing* timing = item->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        uint8_t estabilished_timings1 = edid->GetEstabilishedTimings1();

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_800X600_60HZ)
            add_timing(800, 600, 60);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_800X600_56HZ)
            add_timing(800, 600, 56);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_640X480_75HZ)
            add_timing(640, 480, 75);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_640X480_72HZ)
            add_timing(640, 480, 72);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_640X480_67HZ)
            add_timing(640, 480, 67);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_640X480_60HZ)
            add_timing(640, 480, 60);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_720X400_88HZ)
            add_timing(720, 400, 88);

        if (estabilished_timings1 & EdidParser::ESTABLISHED_TIMINGS_1_720X400_70HZ)
            add_timing(720, 400, 70);

        uint8_t estabilished_timings2 = edid->GetEstabilishedTimings2();

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_1280X1024_75HZ)
            add_timing(1280, 1024, 75);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_1024X768_75HZ)
            add_timing(1024, 768, 75);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_1024X768_70HZ)
            add_timing(1024, 768, 70);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_1024X768_60HZ)
            add_timing(1024, 768, 60);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_1024X768_87HZ)
            add_timing(1024, 768, 87);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_832X624_75HZ)
            add_timing(832, 624, 75);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_800X600_75HZ)
            add_timing(800, 600, 75);

        if (estabilished_timings2 & EdidParser::ESTABLISHED_TIMINGS_2_800X600_72HZ)
            add_timing(800, 600, 72);

        uint8_t manufacturer_timings = edid->GetManufacturersTimings();

        if (manufacturer_timings & EdidParser::MANUFACTURERS_TIMINGS_1152X870_75HZ)
            add_timing(1152, 870, 75);

        for (int index = 0; index < edid->GetStandardTimingsCount(); ++index)
        {
            int width, height, freq;

            if (edid->GetStandardTimings(index, width, height, freq))
                add_timing(width, height, freq);
        }
    }

    return message.SerializeAsString();
}

//
// CategoryOpenGL
//

const char* CategoryOpenGL::Name() const
{
    return "OpenGL";
}

Category::IconId CategoryOpenGL::Icon() const
{
    return IDI_CLAPPERBOARD;
}

const char* CategoryOpenGL::Guid() const
{
    return "05E4437C-A0CD-41CB-8B50-9A627E13CB97";
}

void CategoryOpenGL::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryOpenGL::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupDisplay
//

const char* CategoryGroupDisplay::Name() const
{
    return "Display";
}

Category::IconId CategoryGroupDisplay::Icon() const
{
    return IDI_MONITOR;
}

//
// CategoryPrinters
//

const char* CategoryPrinters::Name() const
{
    return "Printers";
}

Category::IconId CategoryPrinters::Icon() const
{
    return IDI_PRINTER;
}

const char* CategoryPrinters::Guid() const
{
    return "ACBDCE39-CE38-4A79-9626-8C8BA2E3A26A";
}

void CategoryPrinters::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::Printers message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::Printers::Item& item = message.item(index);

        Output::Group group(output, item.name(), Icon());

        output->AddParam(IDI_PRINTER, "Default Printer", item.is_default() ? "Yes" : "No");
        output->AddParam(IDI_PRINTER_SHARE, "Shared Printer", item.is_shared() ? "Yes" : "No");
        output->AddParam(IDI_PORT, "Port", item.port_name());
        output->AddParam(IDI_PCI, "Driver", item.driver_name());
        output->AddParam(IDI_PCI, "Device Name", item.device_name());
        output->AddParam(IDI_PRINTER, "Print Processor", item.print_processor());
        output->AddParam(IDI_PRINTER, "Data Type", item.data_type());
        output->AddParam(IDI_PRINTER, "Print Jobs Queued", std::to_string(item.jobs_count()));

        if (item.paper_width())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Paper Width",
                             std::to_string(item.paper_width()),
                             "mm");
        }

        if (item.paper_length())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Paper Length",
                             std::to_string(item.paper_length()),
                             "mm");
        }

        if (item.print_quality())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Print Quality",
                             std::to_string(item.print_quality()),
                             "dpi");
        }

        switch (item.orientation())
        {
            case system_info::Printers::Item::ORIENTATION_LANDSCAPE:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Landscape");
                break;

            case system_info::Printers::Item::ORIENTATION_PORTRAIT:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Portrait");
                break;

            default:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Unknown");
                break;
        }
    }
}

std::string CategoryPrinters::Serialize()
{
    system_info::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Printers::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_is_default(enumerator.IsDefault());
        item->set_is_shared(enumerator.IsShared());
        item->set_share_name(enumerator.GetShareName());
        item->set_port_name(enumerator.GetPortName());
        item->set_driver_name(enumerator.GetDriverName());
        item->set_device_name(enumerator.GetDeviceName());
        item->set_print_processor(enumerator.GetPrintProcessor());
        item->set_data_type(enumerator.GetDataType());
        item->set_server_name(enumerator.GetServerName());
        item->set_location(enumerator.GetLocation());
        item->set_comment(enumerator.GetComment());
        item->set_jobs_count(enumerator.GetJobsCount());
        item->set_paper_width(enumerator.GetPaperWidth());
        item->set_paper_length(enumerator.GetPaperLength());
        item->set_print_quality(enumerator.GetPrintQuality());

        switch (enumerator.GetOrientation())
        {
            case PrinterEnumerator::Orientation::PORTRAIT:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

//
// CategoryPowerOptions
//

const char* CategoryPowerOptions::Name() const
{
    return "Power Options";
}

Category::IconId CategoryPowerOptions::Icon() const
{
    return IDI_POWER_SUPPLY;
}

const char* CategoryPowerOptions::Guid() const
{
    return "42E04A9E-36F7-42A1-BCDA-F3ED70112DFF";
}

void CategoryPowerOptions::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryPowerOptions::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryAllDevices
//

const char* CategoryAllDevices::Name() const
{
    return "All Devices";
}

Category::IconId CategoryAllDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryAllDevices::Guid() const
{
    return "22C4F1A6-67F2-4445-B807-9D39E1A80636";
}

void CategoryAllDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryAllDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryUnknownDevices
//

const char* CategoryUnknownDevices::Name() const
{
    return "Unknown Devices";
}

Category::IconId CategoryUnknownDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryUnknownDevices::Guid() const
{
    return "5BE9FAA9-5F94-4420-8650-B649F35A1DA0";
}

void CategoryUnknownDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryUnknownDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupWindowDevices
//

const char* CategoryGroupWindowDevices::Name() const
{
    return "Windows Devices";
}

Category::IconId CategoryGroupWindowDevices::Icon() const
{
    return IDI_PCI;
}

//
// CategoryGroupHardware
//

const char* CategoryGroupHardware::Name() const
{
    return "Hardware";
}

Category::IconId CategoryGroupHardware::Icon() const
{
    return IDI_HARDWARE;
}

} // namespace aspia
