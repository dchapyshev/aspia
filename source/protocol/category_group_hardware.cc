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
    system_info::DmiCaches message;

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
        const system_info::DmiCaches::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Cache #%d", index + 1), Icon());

        if (!item.name().empty())
            output->AddParam(IDI_CHIP, "Name", item.name());

        const char* location;
        switch (item.location())
        {
            case system_info::DmiCaches::Item::LOCATION_INTERNAL:
                location = "Internal";
                break;

            case system_info::DmiCaches::Item::LOCATION_EXTERNAL:
                location = "External";
                break;

            case system_info::DmiCaches::Item::LOCATION_RESERVED:
                location = "Reserved";
                break;

            default:
                location = nullptr;
                break;
        }

        if (location != nullptr)
            output->AddParam(IDI_CHIP, "Location", location);

        const char* status;
        switch (item.status())
        {
            case system_info::DmiCaches::Item::STATUS_ENABLED:
                status = "Enabled";
                break;

            case system_info::DmiCaches::Item::STATUS_DISABLED:
                status = "Disabled";
                break;

            default:
                status = nullptr;
                break;
        }

        if (status != nullptr)
            output->AddParam(IDI_CHIP, "Status", status);

        const char* mode;
        switch (item.mode())
        {
            case system_info::DmiCaches::Item::MODE_WRITE_THRU:
                mode = "Write Thru";
                break;

            case system_info::DmiCaches::Item::MODE_WRITE_BACK:
                mode = "Write Back";
                break;

            case system_info::DmiCaches::Item::MODE_WRITE_WITH_MEMORY_ADDRESS:
                mode = "Write with memory address";
                break;

            default:
                mode = nullptr;
                break;
        }

        if (mode != nullptr)
            output->AddParam(IDI_CHIP, "Mode", mode);

        const char* level;
        switch (item.level())
        {
            case system_info::DmiCaches::Item::LEVEL_L1:
                level = "L1";
                break;

            case system_info::DmiCaches::Item::LEVEL_L2:
                level = "L2";
                break;

            case system_info::DmiCaches::Item::LEVEL_L3:
                level = "L3";
                break;

            default:
                level = nullptr;
                break;
        }

        if (level != nullptr)
            output->AddParam(IDI_CHIP, "Level", level);

        if (item.maximum_size() != 0)
            output->AddParam(IDI_CHIP, "Maximum Size", std::to_string(item.maximum_size()), "kB");

        if (item.current_size() != 0)
            output->AddParam(IDI_CHIP, "Current Size", std::to_string(item.current_size()), "kB");

        if (item.supported_sram_types() != 0)
        {
            Output::Group types_group(output, "Supported SRAM Types", Icon());

            auto add_type = [&](system_info::DmiCaches::Item::SRAMType type, const char* name)
            {
                output->AddParam(item.supported_sram_types() & type ? IDI_CHECKED : IDI_UNCHECKED,
                                 name,
                                 item.supported_sram_types() & type ? "Yes" : "No");
            };

            add_type(system_info::DmiCaches::Item::SRAM_TYPE_OTHER, "Other");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_UNKNOWN, "Unknown");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_NON_BURST, "Non-burst");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_BURST, "Burst");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_PIPELINE_BURST, "Pipeline Burst");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_SYNCHRONOUS, "Synchronous");
            add_type(system_info::DmiCaches::Item::SRAM_TYPE_ASYNCHRONOUS, "Asynchronous");
        }

        const char* current_sram_type;
        switch (item.current_sram_type())
        {
            case system_info::DmiCaches::Item::SRAM_TYPE_OTHER:
                current_sram_type = "Other";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_UNKNOWN:
                current_sram_type = "Unknown";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_NON_BURST:
                current_sram_type = "Non-burst";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_BURST:
                current_sram_type = "Burst";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_PIPELINE_BURST:
                current_sram_type = "Pipeline Burst";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_SYNCHRONOUS:
                current_sram_type = "Synchronous";
                break;

            case system_info::DmiCaches::Item::SRAM_TYPE_ASYNCHRONOUS:
                current_sram_type = "Asynchronous";
                break;

            default:
                current_sram_type = nullptr;
                break;
        }

        if (current_sram_type != nullptr)
            output->AddParam(IDI_CHIP, "Current SRAM Type", current_sram_type);

        if (item.speed() != 0)
            output->AddParam(IDI_CHIP, "Speed", std::to_string(item.speed()), "ns");

        const char* ec_type;
        switch (item.error_correction_type())
        {
            case system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_OTHER:
                ec_type = "Other";
                break;

            case system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_NONE:
                ec_type = "None";
                break;

            case system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_PARITY:
                ec_type = "Parity";
                break;

            case system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC:
                ec_type = "Single bit ECC";
                break;

            case system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC:
                ec_type = "Multi bit ECC";
                break;

            default:
                ec_type = nullptr;
                break;
        }

        if (ec_type != nullptr)
            output->AddParam(IDI_CHIP, "Error Correction Type", ec_type);

        const char* type;
        switch (item.type())
        {
            case system_info::DmiCaches::Item::TYPE_OTHER:
                type = "Other";
                break;

            case system_info::DmiCaches::Item::TYPE_INSTRUCTION:
                type = "Instruction";
                break;

            case system_info::DmiCaches::Item::TYPE_DATA:
                type = "Data";
                break;

            case system_info::DmiCaches::Item::TYPE_UNIFIED:
                type = "Unified";
                break;

            default:
                type = nullptr;
                break;
        }

        if (type != nullptr)
            output->AddParam(IDI_CHIP, "Type", type);

        const char* assoc;
        switch (item.associativity())
        {
            case system_info::DmiCaches::Item::ASSOCIATIVITY_OTHER:
                assoc = "Other";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_DIRECT_MAPPED:
                assoc = "Direct Mapped";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_2_WAY:
                assoc = "2-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_4_WAY:
                assoc = "4-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_FULLY:
                assoc = "Fully Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_8_WAY:
                assoc = "8-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_16_WAY:
                assoc = "16-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_12_WAY:
                assoc = "12-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_24_WAY:
                assoc = "24-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_32_WAY:
                assoc = "32-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_48_WAY:
                assoc = "48-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_64_WAY:
                assoc = "64-way Set-Associative";
                break;

            case system_info::DmiCaches::Item::ASSOCIATIVITY_20_WAY:
                assoc = "20-way Set-Associative";
                break;

            default:
                assoc = nullptr;
                break;
        }

        if (assoc != nullptr)
            output->AddParam(IDI_CHIP, "Associativity", assoc);
    }
}

std::string CategoryDmiCaches::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiCaches message;

    for (SMBios::TableEnumerator<SMBios::CacheTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::CacheTable table = table_enumerator.GetTable();
        system_info::DmiCaches::Item* item = message.add_item();

        item->set_name(table.GetName());

        switch (table.GetLocation())
        {
            case SMBios::CacheTable::Location::INTERNAL:
                item->set_location(system_info::DmiCaches::Item::LOCATION_INTERNAL);
                break;

            case SMBios::CacheTable::Location::EXTERNAL:
                item->set_location(system_info::DmiCaches::Item::LOCATION_EXTERNAL);
                break;

            case SMBios::CacheTable::Location::RESERVED:
                item->set_location(system_info::DmiCaches::Item::LOCATION_RESERVED);
                break;

            default:
                break;
        }

        switch (table.GetStatus())
        {
            case SMBios::CacheTable::Status::ENABLED:
                item->set_status(system_info::DmiCaches::Item::STATUS_ENABLED);
                break;

            case SMBios::CacheTable::Status::DISABLED:
                item->set_status(system_info::DmiCaches::Item::STATUS_DISABLED);
                break;

            default:
                break;
        }

        switch (table.GetMode())
        {
            case SMBios::CacheTable::Mode::WRITE_THRU:
                item->set_mode(system_info::DmiCaches::Item::MODE_WRITE_THRU);
                break;

            case SMBios::CacheTable::Mode::WRITE_BACK:
                item->set_mode(system_info::DmiCaches::Item::MODE_WRITE_BACK);
                break;

            case SMBios::CacheTable::Mode::WRITE_WITH_MEMORY_ADDRESS:
                item->set_mode(system_info::DmiCaches::Item::MODE_WRITE_WITH_MEMORY_ADDRESS);
                break;

            default:
                break;
        }

        switch (table.GetLevel())
        {
            case SMBios::CacheTable::Level::L1:
                item->set_level(system_info::DmiCaches::Item::LEVEL_L1);
                break;

            case SMBios::CacheTable::Level::L2:
                item->set_level(system_info::DmiCaches::Item::LEVEL_L2);
                break;

            case SMBios::CacheTable::Level::L3:
                item->set_level(system_info::DmiCaches::Item::LEVEL_L3);
                break;

            default:
                break;
        }

        item->set_maximum_size(table.GetMaximumSize());
        item->set_current_size(table.GetCurrentSize());

        uint16_t supported_sram_types = table.GetSupportedSRAMTypes();

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_OTHER)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_OTHER);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_UNKNOWN)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_UNKNOWN);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_NON_BURST)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_NON_BURST);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_BURST)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_BURST);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_PIPELINE_BURST)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_PIPELINE_BURST);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_SYNCHRONOUS)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_SYNCHRONOUS);
        }

        if (supported_sram_types & SMBios::CacheTable::SRAM_TYPE_ASYNCHRONOUS)
        {
            item->set_supported_sram_types(
                item->supported_sram_types() | system_info::DmiCaches::Item::SRAM_TYPE_ASYNCHRONOUS);
        }

        switch (table.GetCurrentSRAMType())
        {
            case SMBios::CacheTable::SRAM_TYPE_OTHER:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_OTHER);
                break;

            case SMBios::CacheTable::SRAM_TYPE_UNKNOWN:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_UNKNOWN);
                break;

            case SMBios::CacheTable::SRAM_TYPE_NON_BURST:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_NON_BURST);
                break;

            case SMBios::CacheTable::SRAM_TYPE_BURST:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_BURST);
                break;

            case SMBios::CacheTable::SRAM_TYPE_PIPELINE_BURST:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_PIPELINE_BURST);
                break;

            case SMBios::CacheTable::SRAM_TYPE_SYNCHRONOUS:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_SYNCHRONOUS);
                break;

            case SMBios::CacheTable::SRAM_TYPE_ASYNCHRONOUS:
                item->set_current_sram_type(system_info::DmiCaches::Item::SRAM_TYPE_ASYNCHRONOUS);
                break;

            default:
                break;
        }

        item->set_speed(table.GetSpeed());

        switch (table.GetErrorCorrectionType())
        {
            case SMBios::CacheTable::ErrorCorrectionType::OTHER:
                item->set_error_correction_type(system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_OTHER);
                break;

            case SMBios::CacheTable::ErrorCorrectionType::NONE:
                item->set_error_correction_type(system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_NONE);
                break;

            case SMBios::CacheTable::ErrorCorrectionType::PARITY:
                item->set_error_correction_type(system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_PARITY);
                break;

            case SMBios::CacheTable::ErrorCorrectionType::SINGLE_BIT_ECC:
                item->set_error_correction_type(system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_SINGLE_BIT_ECC);
                break;

            case SMBios::CacheTable::ErrorCorrectionType::MULTI_BIT_ECC:
                item->set_error_correction_type(system_info::DmiCaches::Item::ERROR_CORRECTION_TYPE_MULTI_BIT_ECC);
                break;

            default:
                break;
        }

        switch (table.GetType())
        {
            case SMBios::CacheTable::Type::OTHER:
                item->set_type(system_info::DmiCaches::Item::TYPE_OTHER);
                break;

            case SMBios::CacheTable::Type::INSTRUCTION:
                item->set_type(system_info::DmiCaches::Item::TYPE_INSTRUCTION);
                break;

            case SMBios::CacheTable::Type::DATA:
                item->set_type(system_info::DmiCaches::Item::TYPE_DATA);
                break;

            case SMBios::CacheTable::Type::UNIFIED:
                item->set_type(system_info::DmiCaches::Item::TYPE_UNIFIED);
                break;

            default:
                break;
        }

        switch (table.GetAssociativity())
        {
            case SMBios::CacheTable::Associativity::OTHER:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_OTHER);
                break;

            case SMBios::CacheTable::Associativity::DIRECT_MAPPED:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_DIRECT_MAPPED);
                break;

            case SMBios::CacheTable::Associativity::WAY_2_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_2_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_4_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_4_WAY);
                break;

            case SMBios::CacheTable::Associativity::FULLY_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_FULLY);
                break;

            case SMBios::CacheTable::Associativity::WAY_8_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_8_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_16_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_16_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_12_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_12_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_24_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_24_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_32_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_32_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_48_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_48_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_64_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_64_WAY);
                break;

            case SMBios::CacheTable::Associativity::WAY_20_SET_ASSOCIATIVE:
                item->set_associativity(system_info::DmiCaches::Item::ASSOCIATIVITY_20_WAY);
                break;

            default:
                break;
        }
    }

    return message.SerializeAsString();
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
    system_info::DmiProcessors message;

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
        const system_info::DmiProcessors::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Processor #%d", index + 1), Icon());

        if (!item.manufacturer().empty())
            output->AddParam(IDI_PROCESSOR, "Manufacturer", item.manufacturer());

        if (!item.version().empty())
            output->AddParam(IDI_PROCESSOR, "Version", item.version());

        if (!item.family().empty())
            output->AddParam(IDI_PROCESSOR, "Family", item.family());

        const char* type;
        switch (item.type())
        {
            case system_info::DmiProcessors::Item::TYPE_CENTRAL_PROCESSOR:
                type = "Central Processor";
                break;

            case system_info::DmiProcessors::Item::TYPE_MATH_PROCESSOR:
                type = "Math Processor";
                break;

            case system_info::DmiProcessors::Item::TYPE_DSP_PROCESSOR:
                type = "DSP Processor";
                break;

            case system_info::DmiProcessors::Item::TYPE_VIDEO_PROCESSOR:
                type = "Video Processor";
                break;

            case system_info::DmiProcessors::Item::TYPE_OTHER:
                type = "Other Processor";
                break;

            default:
                type = nullptr;
                break;
        }

        if (type != nullptr)
            output->AddParam(IDI_PROCESSOR, "Type", type);

        const char* status;
        switch (item.status())
        {
            case system_info::DmiProcessors::Item::STATUS_ENABLED:
                status = "Enabled";
                break;

            case system_info::DmiProcessors::Item::STATUS_DISABLED_BY_USER:
                status = "Disabled by User";
                break;

            case system_info::DmiProcessors::Item::STATUS_DISABLED_BY_BIOS:
                status = "Disabled by BIOS";
                break;

            case system_info::DmiProcessors::Item::STATUS_IDLE:
                status = "Idle";
                break;

            case system_info::DmiProcessors::Item::STATUS_OTHER:
                status = "Other";
                break;

            default:
                status = nullptr;
                break;
        }

        if (status != nullptr)
            output->AddParam(IDI_PROCESSOR, "Status", status);

        if (!item.socket().empty())
            output->AddParam(IDI_PROCESSOR, "Socket", item.socket());

        if (!item.upgrade().empty())
            output->AddParam(IDI_PROCESSOR, "Upgrade", item.upgrade());

        if (item.external_clock() != 0)
            output->AddParam(IDI_PROCESSOR, "External Clock", std::to_string(item.external_clock()), "MHz");

        if (item.current_speed() != 0)
            output->AddParam(IDI_PROCESSOR, "Current Speed", std::to_string(item.current_speed()), "MHz");

        if (item.maximum_speed() != 0)
            output->AddParam(IDI_PROCESSOR, "Maximum Speed", std::to_string(item.maximum_speed()), "MHz");

        if (item.voltage() != 0.0)
            output->AddParam(IDI_PROCESSOR, "Voltage", std::to_string(item.voltage()), "V");

        if (!item.serial_number().empty())
            output->AddParam(IDI_PROCESSOR, "Serial Number", item.serial_number());

        if (!item.asset_tag().empty())
        output->AddParam(IDI_PROCESSOR, "Asset Tag", item.asset_tag());

        if (!item.part_number().empty())
            output->AddParam(IDI_PROCESSOR, "Part Number", item.part_number());

        if (item.core_count() != 0)
            output->AddParam(IDI_PROCESSOR, "Core Count", std::to_string(item.core_count()));

        if (item.core_enabled() != 0)
            output->AddParam(IDI_PROCESSOR, "Core Enabled", std::to_string(item.core_enabled()));

        if (item.thread_count() != 0)
            output->AddParam(IDI_PROCESSOR, "Thread Count", std::to_string(item.thread_count()));

        auto add_characteristic = [&](uint32_t flag, const char* name)
        {
            output->AddParam((item.characteristics() & flag) ? IDI_CHECKED : IDI_UNCHECKED,
                             name,
                             (item.characteristics() & flag) ? "Yes" : "No");
        };

        if (item.characteristics() != 0)
        {
            Output::Group characteristic_group(output, "Characteristics", IDI_PROCESSOR);

            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_64BIT_CAPABLE,
                               "64-bit Capable");
            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_MULTI_CORE,
                               "Multi-Core");
            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_HARDWARE_THREAD,
                               "Hardware Thread");
            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_EXECUTE_PROTECTION,
                               "Execute Protection");
            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_ENHANCED_VIRTUALIZATION,
                               "Enhanced Virtualization");
            add_characteristic(system_info::DmiProcessors::Item::CHARACTERISTIC_POWER_CONTROL,
                               "Power/Perfomance Control");
        }
    }
}

std::string CategoryDmiProcessors::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiProcessors message;

    for (SMBios::TableEnumerator<SMBios::ProcessorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ProcessorTable table = table_enumerator.GetTable();
        system_info::DmiProcessors::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_family(table.GetFamily());

        switch (table.GetType())
        {
            case SMBios::ProcessorTable::Type::CENTRAL_PROCESSOR:
                item->set_type(system_info::DmiProcessors::Item::TYPE_CENTRAL_PROCESSOR);
                break;

            case SMBios::ProcessorTable::Type::MATH_PROCESSOR:
                item->set_type(system_info::DmiProcessors::Item::TYPE_MATH_PROCESSOR);
                break;

            case SMBios::ProcessorTable::Type::DSP_PROCESSOR:
                item->set_type(system_info::DmiProcessors::Item::TYPE_DSP_PROCESSOR);
                break;

            case SMBios::ProcessorTable::Type::VIDEO_PROCESSOR:
                item->set_type(system_info::DmiProcessors::Item::TYPE_VIDEO_PROCESSOR);
                break;

            case SMBios::ProcessorTable::Type::OTHER:
                item->set_type(system_info::DmiProcessors::Item::TYPE_OTHER);
                break;

            default:
                break;
        }

        switch (table.GetStatus())
        {
            case SMBios::ProcessorTable::Status::ENABLED:
                item->set_status(system_info::DmiProcessors::Item::STATUS_ENABLED);
                break;

            case SMBios::ProcessorTable::Status::DISABLED_BY_USER:
                item->set_status(system_info::DmiProcessors::Item::STATUS_DISABLED_BY_USER);
                break;

            case SMBios::ProcessorTable::Status::DISABLED_BY_BIOS:
                item->set_status(system_info::DmiProcessors::Item::STATUS_DISABLED_BY_BIOS);
                break;

            case SMBios::ProcessorTable::Status::IDLE:
                item->set_status(system_info::DmiProcessors::Item::STATUS_IDLE);
                break;

            case SMBios::ProcessorTable::Status::OTHER:
                item->set_status(system_info::DmiProcessors::Item::STATUS_OTHER);
                break;

            default:
                break;
        }

        item->set_socket(table.GetSocket());
        item->set_upgrade(table.GetUpgrade());
        item->set_external_clock(table.GetExternalClock());
        item->set_current_speed(table.GetCurrentSpeed());
        item->set_maximum_speed(table.GetMaximumSpeed());
        item->set_voltage(table.GetVoltage());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_part_number(table.GetPartNumber());
        item->set_core_count(table.GetCoreCount());
        item->set_core_enabled(table.GetCoreEnabled());
        item->set_thread_count(table.GetThreadCount());

        auto add_characteristic = [&](uint16_t src, uint32_t dst)
        {
            uint16_t bitfield = table.GetCharacteristics();

            if (bitfield & src)
            {
                item->set_characteristics(item->characteristics() | dst);
            }
        };

        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_64BIT_CAPABLE,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_64BIT_CAPABLE);
        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_MULTI_CORE,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_MULTI_CORE);
        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_HARDWARE_THREAD,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_HARDWARE_THREAD);
        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_EXECUTE_PROTECTION,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_EXECUTE_PROTECTION);
        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_ENHANCED_VIRTUALIZATION,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_ENHANCED_VIRTUALIZATION);
        add_characteristic(SMBios::ProcessorTable::CHARACTERISTIC_POWER_CONTROL,
                           system_info::DmiProcessors::Item::CHARACTERISTIC_POWER_CONTROL);
    }

    return message.SerializeAsString();
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
    system_info::DmiMemoryDevices message;

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
        const system_info::DmiMemoryDevices::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Memory Device #%d", index + 1), Icon());

        if (!item.device_locator().empty())
            output->AddParam(IDI_MEMORY, "Device Locator", item.device_locator());

        if (item.size() != 0)
            output->AddParam(IDI_MEMORY, "Size", std::to_string(item.size()), "MB");

        if (!item.type().empty())
            output->AddParam(IDI_MEMORY, "Type", item.type());

        if (item.speed() != 0)
            output->AddParam(IDI_MEMORY, "Speed", std::to_string(item.speed()), "MHz");

        if (!item.form_factor().empty())
            output->AddParam(IDI_MEMORY, "Form Factor", item.form_factor());

        if (!item.serial_number().empty())
            output->AddParam(IDI_MEMORY, "Serial Number", item.serial_number());

        if (!item.part_number().empty())
            output->AddParam(IDI_MEMORY, "Part Number", item.part_number());

        if (!item.manufactorer().empty())
            output->AddParam(IDI_MEMORY, "Manufacturer", item.manufactorer());

        if (!item.bank().empty())
            output->AddParam(IDI_MEMORY, "Bank", item.bank());

        if (item.total_width() != 0)
            output->AddParam(IDI_MEMORY, "Total Width", std::to_string(item.total_width()), "Bit");

        if (item.data_width() != 0)
            output->AddParam(IDI_MEMORY, "Data Width", std::to_string(item.data_width()), "Bit");
    }
}

std::string CategoryDmiMemoryDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiMemoryDevices message;

    for (SMBios::TableEnumerator<SMBios::MemoryDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::MemoryDeviceTable table = table_enumerator.GetTable();
        system_info::DmiMemoryDevices::Item* item = message.add_item();

        item->set_device_locator(table.GetDeviceLocator());
        item->set_size(table.GetSize());
        item->set_type(table.GetType());
        item->set_speed(table.GetSpeed());
        item->set_form_factor(table.GetFormFactor());
        item->set_serial_number(table.GetSerialNumber());
        item->set_part_number(table.GetPartNumber());
        item->set_manufactorer(table.GetManufacturer());
        item->set_bank(table.GetBank());
        item->set_total_width(table.GetTotalWidth());
        item->set_data_width(table.GetDataWidth());
    }

    return message.SerializeAsString();
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
    system_info::DmiSystemSlots message;

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
        const system_info::DmiSystemSlots::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("System Slot #%d", index + 1), Icon());

        if (!item.slot_designation().empty())
            output->AddParam(IDI_PORT, "Slot Designation", item.slot_designation());

        if (!item.type().empty())
            output->AddParam(IDI_PORT, "Type", item.type());

        const char* usage;
        switch (item.usage())
        {
            case system_info::DmiSystemSlots::Item::USAGE_OTHER:
                usage = "Other";
                break;

            case system_info::DmiSystemSlots::Item::USAGE_AVAILABLE:
                usage = "Available";
                break;

            case system_info::DmiSystemSlots::Item::USAGE_IN_USE:
                usage = "In Use";
                break;

            default:
                usage = nullptr;
                break;
        }

        if (usage != nullptr)
            output->AddParam(IDI_PORT, "Usage", usage);

        if (!item.bus_width().empty())
            output->AddParam(IDI_PORT, "Bus Width", item.bus_width());

        const char* length;
        switch (item.length())
        {
            case system_info::DmiSystemSlots::Item::LENGTH_OTHER:
                length = "Other";
                break;

            case system_info::DmiSystemSlots::Item::LENGTH_SHORT:
                length = "Short";
                break;

            case system_info::DmiSystemSlots::Item::LENGTH_LONG:
                length = "Long";
                break;

            default:
                length = nullptr;
                break;
        }

        if (length != nullptr)
            output->AddParam(IDI_PORT, "Length", length);
    }
}

std::string CategoryDmiSystemSlots::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiSystemSlots message;

    for (SMBios::TableEnumerator<SMBios::SystemSlotTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::SystemSlotTable table = table_enumerator.GetTable();
        system_info::DmiSystemSlots::Item* item = message.add_item();

        item->set_slot_designation(table.GetSlotDesignation());
        item->set_type(table.GetType());

        switch (table.GetUsage())
        {
            case SMBios::SystemSlotTable::Usage::OTHER:
                item->set_usage(system_info::DmiSystemSlots::Item::USAGE_OTHER);
                break;

            case SMBios::SystemSlotTable::Usage::AVAILABLE:
                item->set_usage(system_info::DmiSystemSlots::Item::USAGE_AVAILABLE);
                break;

            case SMBios::SystemSlotTable::Usage::IN_USE:
                item->set_usage(system_info::DmiSystemSlots::Item::USAGE_IN_USE);
                break;

            default:
                break;
        }

        item->set_bus_width(table.GetBusWidth());

        switch (table.GetLength())
        {
            case SMBios::SystemSlotTable::Length::OTHER:
                item->set_length(system_info::DmiSystemSlots::Item::LENGTH_OTHER);
                break;

            case SMBios::SystemSlotTable::Length::SHORT:
                item->set_length(system_info::DmiSystemSlots::Item::LENGTH_SHORT);
                break;

            case SMBios::SystemSlotTable::Length::LONG:
                item->set_length(system_info::DmiSystemSlots::Item::LENGTH_LONG);
                break;

            default:
                break;
        }
    }

    return message.SerializeAsString();
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
    system_info::DmiPortConnectors message;

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
        const system_info::DmiPortConnectors::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Port Connector #%d", index + 1), Icon());

        if (!item.internal_designation().empty())
            output->AddParam(IDI_PORT, "Internal Designation", item.internal_designation());

        if (!item.external_designation().empty())
            output->AddParam(IDI_PORT, "External Designation", item.external_designation());

        if (!item.type().empty())
            output->AddParam(IDI_PORT, "Type", item.type());

        if (!item.internal_connector_type().empty())
            output->AddParam(IDI_PORT, "Internal Connector Type", item.internal_connector_type());

        if (!item.external_connector_type().empty())
            output->AddParam(IDI_PORT, "External Connector Type", item.external_connector_type());
    }
}

std::string CategoryDmiPortConnectors::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiPortConnectors message;

    for (SMBios::TableEnumerator<SMBios::PortConnectorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortConnectorTable table = table_enumerator.GetTable();
        system_info::DmiPortConnectors::Item* item = message.add_item();

        item->set_internal_designation(table.GetInternalDesignation());
        item->set_external_designation(table.GetExternalDesignation());
        item->set_type(table.GetType());
        item->set_internal_connector_type(table.GetInternalConnectorType());
        item->set_external_connector_type(table.GetExternalConnectorType());
    }

    return message.SerializeAsString();
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
    system_info::DmiOnBoardDevices message;

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
        const system_info::DmiOnBoardDevices::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("OnBoard Device #%d", index + 1), Icon());

        if (!item.description().empty())
            output->AddParam(IDI_MOTHERBOARD, "Description", item.description());

        if (!item.type().empty())
            output->AddParam(IDI_MOTHERBOARD, "Type", item.type());

        output->AddParam(IDI_MOTHERBOARD, "Status", item.enabled() ? "Enabled" : "Disabled");
    }
}

std::string CategoryDmiOnboardDevices::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiOnBoardDevices message;

    for (SMBios::TableEnumerator<SMBios::OnBoardDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::OnBoardDeviceTable table = table_enumerator.GetTable();

        for (int index = 0; index < table.GetDeviceCount(); ++index)
        {
            system_info::DmiOnBoardDevices::Item* item = message.add_item();

            item->set_description(table.GetDescription(index));
            item->set_type(table.GetType(index));
            item->set_enabled(table.IsEnabled(index));
        }
    }

    return message.SerializeAsString();
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
    system_info::DmiBuildinPointing message;

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
        const system_info::DmiBuildinPointing::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Build-in Pointing Device #%d", index + 1), Icon());

        if (!item.device_type().empty())
            output->AddParam(IDI_MOUSE, "Device Type", item.device_type());

        if (!item.device_interface().empty())
            output->AddParam(IDI_MOUSE, "Device Interface", item.device_interface());

        if (item.button_count() != 0)
            output->AddParam(IDI_MOUSE, "Buttons Count", std::to_string(item.button_count()));
    }
}

std::string CategoryDmiBuildinPointing::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiBuildinPointing message;

    for (SMBios::TableEnumerator<SMBios::BuildinPointingTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::BuildinPointingTable table = table_enumerator.GetTable();
        system_info::DmiBuildinPointing::Item* item = message.add_item();

        item->set_device_type(table.GetDeviceType());
        item->set_device_interface(table.GetInterface());
        item->set_button_count(table.GetButtonCount());
    }

    return message.SerializeAsString();
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
    system_info::DmiPortableBattery message;

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
        const system_info::DmiPortableBattery::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Portable Battery #%d", index + 1), Icon());

        if (!item.location().empty())
            output->AddParam(IDI_BATTERY, "Location", item.location());

        if (!item.manufacturer().empty())
            output->AddParam(IDI_BATTERY, "Manufacturer", item.manufacturer());

        if (!item.manufacture_date().empty())
            output->AddParam(IDI_BATTERY, "Manufacture Date", item.manufacture_date());

        if (!item.sbds_serial_number().empty())
            output->AddParam(IDI_BATTERY, "Serial Number", item.sbds_serial_number());

        if (!item.device_name().empty())
            output->AddParam(IDI_BATTERY, "Device Name", item.device_name());

        if (!item.chemistry().empty())
            output->AddParam(IDI_BATTERY, "Chemistry", item.chemistry());

        if (item.design_capacity() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Design Capacity",
                             std::to_string(item.design_capacity()),
                             "mWh");
        }

        if (item.design_voltage() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Design Voltage",
                             std::to_string(item.design_voltage()),
                             "mV");
        }

        if (item.max_error_in_battery_data() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Max. Error in Battery Data",
                             std::to_string(item.max_error_in_battery_data()),
                             "%%");
        }

        if (!item.sbds_version_number().empty())
            output->AddParam(IDI_BATTERY, "SBDS Version Number", item.sbds_version_number());

        if (!item.sbds_serial_number().empty())
            output->AddParam(IDI_BATTERY, "SBDS Serial Number", item.sbds_serial_number());

        if (!item.sbds_manufacture_date().empty())
            output->AddParam(IDI_BATTERY, "SBDS Manufacture Date", item.sbds_manufacture_date());

        if (!item.sbds_device_chemistry().empty())
            output->AddParam(IDI_BATTERY, "SBDS Device Chemistry", item.sbds_device_chemistry());
    }
}

std::string CategoryDmiPortableBattery::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    system_info::DmiPortableBattery message;

    for (SMBios::TableEnumerator<SMBios::PortableBatteryTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortableBatteryTable table = table_enumerator.GetTable();
        system_info::DmiPortableBattery::Item* item = message.add_item();

        item->set_location(table.GetLocation());
        item->set_manufacturer(table.GetManufacturer());
        item->set_manufacture_date(table.GetManufactureDate());
        item->set_serial_number(table.GetSerialNumber());
        item->set_device_name(table.GetDeviceName());
        item->set_chemistry(table.GetChemistry());
        item->set_design_capacity(table.GetDesignCapacity());
        item->set_design_voltage(table.GetDesignVoltage());
        item->set_sbds_version_number(table.GetSBDSVersionNumber());
        item->set_max_error_in_battery_data(table.GetMaxErrorInBatteryData());
        item->set_sbds_serial_number(table.GetSBDSSerialNumber());
        item->set_sbds_manufacture_date(table.GetSBDSManufactureDate());
        item->set_sbds_device_chemistry(table.GetSBDSDeviceChemistry());
    }

    return message.SerializeAsString();
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
// CategoryWindowsDevices
//

const char* CategoryWindowsDevices::Name() const
{
    return "Windows Devices";
}

Category::IconId CategoryWindowsDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryWindowsDevices::Guid() const
{
    return "22C4F1A6-67F2-4445-B807-9D39E1A80636";
}

void CategoryWindowsDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::WindowsDevices message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Device Name", 200);
        output->AddHeaderItem("Driver Version", 90);
        output->AddHeaderItem("Driver Date", 80);
        output->AddHeaderItem("Driver Vendor", 90);
        output->AddHeaderItem("Device ID", 200);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::WindowsDevices::Item& item = message.item(index);

        Output::Row row(output, Icon());

        if (!item.friendly_name().empty())
            output->AddValue(item.friendly_name());
        else if (!item.description().empty())
            output->AddValue(item.description());
        else
            output->AddValue("Unknown Device");

        output->AddValue(item.driver_version());
        output->AddValue(item.driver_date());
        output->AddValue(item.driver_vendor());
        output->AddValue(item.device_id());
    }
}

std::string CategoryWindowsDevices::Serialize()
{
    system_info::WindowsDevices message;

    for (DeviceEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::WindowsDevices::Item* item = message.add_item();

        item->set_friendly_name(enumerator.GetFriendlyName());
        item->set_description(enumerator.GetDescription());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_vendor(enumerator.GetDriverVendor());
        item->set_device_id(enumerator.GetDeviceID());
    }

    return message.SerializeAsString();
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
