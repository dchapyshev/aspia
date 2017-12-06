//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/devices/battery_enumerator.h"
#include "base/devices/monitor_enumerator.h"
#include "base/devices/video_adapter_enumarator.h"
#include "base/devices/physical_drive_enumerator.h"
#include "base/devices/smbios_reader.h"
#include "base/strings/string_util.h"
#include "base/cpu_info.h"
#include "protocol/category_group_hardware.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output.h"
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

void CategoryDmiBios::Parse(Output* output, const std::string& data)
{
    proto::DmiBios message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

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
            const proto::DmiBios::Feature& feature = message.feature(index);

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
    proto::DmiBios message;

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
        proto::DmiBios::Feature* item = message.add_feature();
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

void CategoryDmiSystem::Parse(Output* output, const std::string& data)
{
    proto::DmiSystem message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

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
    proto::DmiSystem message;

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

void CategoryDmiBaseboard::Parse(Output* output, const std::string& data)
{
    proto::DmiBaseboard message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiBaseboard::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Baseboard #%d", index + 1), Icon());

        if (!item.type().empty())
            output->AddParam(IDI_MOTHERBOARD, "Type", item.type());

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
                const proto::DmiBaseboard::Item::Feature& feature = item.feature(i);

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

    proto::DmiBaseboard message;

    for (SMBios::TableEnumerator<SMBios::BaseboardTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::BaseboardTable table = table_enumerator.GetTable();
        proto::DmiBaseboard::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_product_name(table.GetProductName());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_location_in_chassis(table.GetLocationInChassis());
        item->set_type(table.GetBoardType());

        SMBios::BaseboardTable::FeatureList feature_list = table.GetFeatures();

        for (const auto& feature : feature_list)
        {
            auto feature_item = item->add_feature();
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

void CategoryDmiChassis::Parse(Output* output, const std::string& data)
{
    proto::DmiChassis message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiChassis::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Chassis #%d", index + 1), Icon());

        if (!item.manufacturer().empty())
            output->AddParam(IDI_SERVER, "Manufacturer", item.manufacturer());

        if (!item.version().empty())
            output->AddParam(IDI_SERVER, "Version", item.version());

        if (!item.serial_number().empty())
            output->AddParam(IDI_SERVER, "Serial Number", item.serial_number());

        if (!item.asset_tag().empty())
            output->AddParam(IDI_SERVER, "Asset Tag", item.asset_tag());

        if (!item.type().empty())
            output->AddParam(IDI_SERVER, "Type", item.type());

        if (!item.os_load_status().empty())
            output->AddParam(IDI_SERVER, "OS Load Status", item.os_load_status());

        if (!item.power_source_status().empty())
            output->AddParam(IDI_SERVER, "Power Source Status", item.power_source_status());

        if (!item.temparature_status().empty())
            output->AddParam(IDI_SERVER, "Temperature Status", item.temparature_status());

        if (!item.security_status().empty())
            output->AddParam(IDI_SERVER, "Security Status", item.security_status());

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

    proto::DmiChassis message;

    for (SMBios::TableEnumerator<SMBios::ChassisTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ChassisTable table = table_enumerator.GetTable();
        proto::DmiChassis::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_serial_number(table.GetSerialNumber());
        item->set_asset_tag(table.GetAssetTag());
        item->set_type(table.GetType());
        item->set_os_load_status(table.GetOSLoadStatus());
        item->set_power_source_status(table.GetPowerSourceStatus());
        item->set_temparature_status(table.GetTemperatureStatus());
        item->set_security_status(table.GetSecurityStatus());
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

void CategoryDmiCaches::Parse(Output* output, const std::string& data)
{
    proto::DmiCaches message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiCaches::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Cache #%d", index + 1), Icon());

        if (!item.name().empty())
            output->AddParam(IDI_CHIP, "Name", item.name());

        if (!item.location().empty())
            output->AddParam(IDI_CHIP, "Location", item.location());

        output->AddParam(IDI_CHIP, "Status", item.enabled() ? "Enabled" : "Disabled");

        if (!item.mode().empty())
            output->AddParam(IDI_CHIP, "Mode", item.mode());

        if (item.level() != 0)
            output->AddParam(IDI_CHIP, "Level", StringPrintf("L%d", item.level()));

        if (item.maximum_size() != 0)
            output->AddParam(IDI_CHIP, "Maximum Size", std::to_string(item.maximum_size()), "kB");

        if (item.current_size() != 0)
            output->AddParam(IDI_CHIP, "Current Size", std::to_string(item.current_size()), "kB");

        if (item.supported_sram_type_size())
        {
            Output::Group types_group(output, "Supported SRAM Types", Icon());

            for (int i = 0; i < item.supported_sram_type_size(); ++i)
            {
                output->AddParam(item.supported_sram_type(i).supported() ? IDI_CHECKED : IDI_UNCHECKED,
                                 item.supported_sram_type(i).name(),
                                 item.supported_sram_type(i).supported() ? "Yes" : "No");
            }
        }

        if (!item.current_sram_type().empty())
            output->AddParam(IDI_CHIP, "Current SRAM Type", item.current_sram_type());

        if (item.speed() != 0)
            output->AddParam(IDI_CHIP, "Speed", std::to_string(item.speed()), "ns");

        if (!item.error_correction_type().empty())
            output->AddParam(IDI_CHIP, "Error Correction Type", item.error_correction_type());

        if (!item.type().empty())
            output->AddParam(IDI_CHIP, "Type", item.type());

        if (!item.associativity().empty())
            output->AddParam(IDI_CHIP, "Associativity", item.associativity());
    }
}

std::string CategoryDmiCaches::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiCaches message;

    for (SMBios::TableEnumerator<SMBios::CacheTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::CacheTable table = table_enumerator.GetTable();
        proto::DmiCaches::Item* item = message.add_item();

        item->set_name(table.GetName());
        item->set_location(table.GetLocation());
        item->set_enabled(table.IsEnabled());
        item->set_mode(table.GetMode());
        item->set_level(table.GetLevel());
        item->set_maximum_size(table.GetMaximumSize());
        item->set_current_size(table.GetCurrentSize());

        for (const auto& sram_type : table.GetSupportedSRAMTypes())
        {
            auto sram_type_item = item->add_supported_sram_type();
            sram_type_item->set_name(sram_type.first);
            sram_type_item->set_supported(sram_type.second);
        }

        item->set_current_sram_type(table.GetCurrentSRAMType());
        item->set_speed(table.GetSpeed());
        item->set_error_correction_type(table.GetErrorCorrectionType());
        item->set_type(table.GetType());
        item->set_associativity(table.GetAssociativity());
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

void CategoryDmiProcessors::Parse(Output* output, const std::string& data)
{
    proto::DmiProcessors message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiProcessors::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("Processor #%d", index + 1), Icon());

        if (!item.manufacturer().empty())
            output->AddParam(IDI_PROCESSOR, "Manufacturer", item.manufacturer());

        if (!item.version().empty())
            output->AddParam(IDI_PROCESSOR, "Version", item.version());

        if (!item.family().empty())
            output->AddParam(IDI_PROCESSOR, "Family", item.family());

        if (!item.type().empty())
            output->AddParam(IDI_PROCESSOR, "Type", item.type());

        if (!item.status().empty())
            output->AddParam(IDI_PROCESSOR, "Status", item.status());

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

        if (item.feature_size())
        {
            Output::Group features_group(output, "Features", IDI_PROCESSOR);

            for (int i = 0; i < item.feature_size(); ++i)
            {
                output->AddParam(item.feature(i).supported() ? IDI_CHECKED : IDI_UNCHECKED,
                                 item.feature(i).name(),
                                 item.feature(i).supported() ? "Yes" : "No");
            }
        }
    }
}

std::string CategoryDmiProcessors::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiProcessors message;

    for (SMBios::TableEnumerator<SMBios::ProcessorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::ProcessorTable table = table_enumerator.GetTable();
        proto::DmiProcessors::Item* item = message.add_item();

        item->set_manufacturer(table.GetManufacturer());
        item->set_version(table.GetVersion());
        item->set_family(table.GetFamily());
        item->set_type(table.GetType());
        item->set_status(table.GetStatus());
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

        for (const auto& feature : table.GetFeatures())
        {
            auto feature_item = item->add_feature();
            feature_item->set_name(feature.first);
            feature_item->set_supported(feature.second);
        }
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

void CategoryDmiMemoryDevices::Parse(Output* output, const std::string& data)
{
    proto::DmiMemoryDevices message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiMemoryDevices::Item& item = message.item(index);

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

    proto::DmiMemoryDevices message;

    for (SMBios::TableEnumerator<SMBios::MemoryDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::MemoryDeviceTable table = table_enumerator.GetTable();
        proto::DmiMemoryDevices::Item* item = message.add_item();

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

void CategoryDmiSystemSlots::Parse(Output* output, const std::string& data)
{
    proto::DmiSystemSlots message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiSystemSlots::Item& item = message.item(index);

        Output::Group group(output, StringPrintf("System Slot #%d", index + 1), Icon());

        if (!item.slot_designation().empty())
            output->AddParam(IDI_PORT, "Slot Designation", item.slot_designation());

        if (!item.type().empty())
            output->AddParam(IDI_PORT, "Type", item.type());

        if (!item.usage().empty())
            output->AddParam(IDI_PORT, "Usage", item.usage());

        if (!item.bus_width().empty())
            output->AddParam(IDI_PORT, "Bus Width", item.bus_width());

        if (!item.length().empty())
            output->AddParam(IDI_PORT, "Length", item.length());
    }
}

std::string CategoryDmiSystemSlots::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiSystemSlots message;

    for (SMBios::TableEnumerator<SMBios::SystemSlotTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::SystemSlotTable table = table_enumerator.GetTable();
        proto::DmiSystemSlots::Item* item = message.add_item();

        item->set_slot_designation(table.GetSlotDesignation());
        item->set_type(table.GetType());
        item->set_usage(table.GetUsage());
        item->set_bus_width(table.GetBusWidth());
        item->set_length(table.GetLength());
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

void CategoryDmiPortConnectors::Parse(Output* output, const std::string& data)
{
    proto::DmiPortConnectors message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortConnectors::Item& item = message.item(index);

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

    proto::DmiPortConnectors message;

    for (SMBios::TableEnumerator<SMBios::PortConnectorTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortConnectorTable table = table_enumerator.GetTable();
        proto::DmiPortConnectors::Item* item = message.add_item();

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

void CategoryDmiOnboardDevices::Parse(Output* output, const std::string& data)
{
    proto::DmiOnBoardDevices message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiOnBoardDevices::Item& item = message.item(index);

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

    proto::DmiOnBoardDevices message;

    for (SMBios::TableEnumerator<SMBios::OnBoardDeviceTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::OnBoardDeviceTable table = table_enumerator.GetTable();

        for (int index = 0; index < table.GetDeviceCount(); ++index)
        {
            proto::DmiOnBoardDevices::Item* item = message.add_item();

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

void CategoryDmiBuildinPointing::Parse(Output* output, const std::string& data)
{
    proto::DmiBuildinPointing message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiBuildinPointing::Item& item = message.item(index);

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

    proto::DmiBuildinPointing message;

    for (SMBios::TableEnumerator<SMBios::BuildinPointingTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::BuildinPointingTable table = table_enumerator.GetTable();
        proto::DmiBuildinPointing::Item* item = message.add_item();

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

void CategoryDmiPortableBattery::Parse(Output* output, const std::string& data)
{
    proto::DmiPortableBattery message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortableBattery::Item& item = message.item(index);

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

    proto::DmiPortableBattery message;

    for (SMBios::TableEnumerator<SMBios::PortableBatteryTable> table_enumerator(*smbios);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::PortableBatteryTable table = table_enumerator.GetTable();
        proto::DmiPortableBattery::Item* item = message.add_item();

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

void CategoryCPU::Parse(Output* output, const std::string& data)
{
    proto::CentralProcessor message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    output->AddParam(IDI_PROCESSOR, "Brand String", message.brand_string());
    output->AddParam(IDI_PROCESSOR, "Vendor", message.vendor());
    output->AddParam(IDI_PROCESSOR, "Stepping", StringPrintf("%02Xh", message.stepping()));
    output->AddParam(IDI_PROCESSOR, "Model", StringPrintf("%02Xh", message.model()));
    output->AddParam(IDI_PROCESSOR, "Family", StringPrintf("%02Xh", message.family()));

    if (message.extended_model())
    {
        output->AddParam(IDI_PROCESSOR,
                         "Extended Model",
                         StringPrintf("%02Xh", message.extended_model()));
    }

    if (message.extended_family())
    {
        output->AddParam(IDI_PROCESSOR,
                         "Extended Family",
                         StringPrintf("%02Xh", message.extended_family()));
    }

    output->AddParam(IDI_PROCESSOR, "Brand ID", StringPrintf("%02Xh", message.brand_id()));
    output->AddParam(IDI_PROCESSOR, "Packages", std::to_string(message.packages()));
    output->AddParam(IDI_PROCESSOR, "Physical Cores", std::to_string(message.physical_cores()));
    output->AddParam(IDI_PROCESSOR, "Logical Cores", std::to_string(message.logical_cores()));

    if (message.feature_size())
    {
        Output::Group group(output, "Features", Icon());

        for (int i = 0; i < message.feature_size(); ++i)
        {
            const proto::CentralProcessor::Feature& feature = message.feature(i);

            output->AddParam(feature.supported() ? IDI_CHECKED : IDI_UNCHECKED,
                             feature.name(),
                             feature.supported() ? "Yes" : "No");
        }
    }
}

std::string CategoryCPU::Serialize()
{
    proto::CentralProcessor message;

    CPUInfo cpu;
    GetCPUInformation(cpu);

    message.set_brand_string(cpu.brand_string);
    message.set_vendor(cpu.vendor);
    message.set_stepping(cpu.stepping);
    message.set_model(cpu.model);
    message.set_family(cpu.family);
    message.set_extended_model(cpu.extended_model);
    message.set_extended_family(cpu.extended_family);
    message.set_brand_id(cpu.brand_id);
    message.set_packages(cpu.package_count);
    message.set_physical_cores(cpu.physical_core_count);
    message.set_logical_cores(cpu.logical_core_count);

    std::vector<std::pair<std::string, bool>> features;

    auto add = [&](uint8_t flag, const char* name)
    {
        features.emplace_back(name, flag ? true : false);
    };

    // Function 1 EDX
    add(cpu.fn_1_edx.has_fpu, "Floating-point Unit On-Chip (FPU)");
    add(cpu.fn_1_edx.has_vme, "Virtual Mode Extension (VME)");
    add(cpu.fn_1_edx.has_de, "Debug Extension (DE)");
    add(cpu.fn_1_edx.has_pse, "Page Size Extension (PSE)");
    add(cpu.fn_1_edx.has_tsc, "Time Stamp Extension (TSC)");
    add(cpu.fn_1_edx.has_msr, "Model Specific Registers (MSR)");
    add(cpu.fn_1_edx.has_pae, "Physical Address Extension (PAE)");
    add(cpu.fn_1_edx.has_mce, "Machine-Check Exception (MCE)");
    add(cpu.fn_1_edx.has_cx8, "CMPXCHG8 Instruction (CX8)");
    add(cpu.fn_1_edx.has_apic, "On-cpip APIC Hardware (APIC)");
    add(cpu.fn_1_edx.has_sep, "Fast System Call (SEP)");
    add(cpu.fn_1_edx.has_mtrr, "Memory Type Range Registers (MTRR)");
    add(cpu.fn_1_edx.has_pge, "Page Global Enable (PGE)");
    add(cpu.fn_1_edx.has_mca, "Machine-Check Architecture (MCA)");
    add(cpu.fn_1_edx.has_cmov, "Conditional Move Instruction (CMOV)");
    add(cpu.fn_1_edx.has_pat, "Page Attribute Table (PAT)");
    add(cpu.fn_1_edx.has_pse36, "36-bit Page Size Extension (PSE36)");
    add(cpu.fn_1_edx.has_psn, "Processor Serial Number (PSN)");
    add(cpu.fn_1_edx.has_clfsh, "CLFLUSH Instruction");
    add(cpu.fn_1_edx.has_ds, "Debug Store (DS)");
    add(cpu.fn_1_edx.has_acpu, "Thermal Monitor and Software Controlled Clock Facilities (ACPU)");
    add(cpu.fn_1_edx.has_mmx, "MMX Technology (MMX)");
    add(cpu.fn_1_edx.has_fxsr, "FXSAVE and FXSTOR Instructions (FXSR)");
    add(cpu.fn_1_edx.has_sse, "Streaming SIMD Extension (SSE)");
    add(cpu.fn_1_edx.has_sse2, "Streaming SIMD Extension 2 (SSE2)");
    add(cpu.fn_1_edx.has_ss, "Self-Snoop (SS)");
    add(cpu.fn_1_edx.has_htt, "Multi-Threading (HTT)");
    add(cpu.fn_1_edx.has_tm, "Thermal Monitor (TM)");
    add(cpu.fn_1_edx.has_ia64, "IA64 Processor Emulating x86");
    add(cpu.fn_1_edx.has_pbe, "Pending Break Enable (PBE)");

    // Function 1 ECX
    add(cpu.fn_1_ecx.has_sse3, "Streaming SIMD Extension 3 (SSE3)");
    add(cpu.fn_1_ecx.has_pclmuldq, "PCLMULDQ Instruction");
    add(cpu.fn_1_ecx.has_dtes64, "64-Bit Debug Store (DTES64)");
    add(cpu.fn_1_ecx.has_monitor, "MONITOR/MWAIT Instructions");
    add(cpu.fn_1_ecx.has_ds_cpl, "CPL Qualified Debug Store");
    add(cpu.fn_1_ecx.has_vmx, "Virtual Machine Extensions (VMX, Vanderpool)");
    add(cpu.fn_1_ecx.has_smx, "Safe Mode Extensions (SMX)");
    add(cpu.fn_1_ecx.has_est, "Enhanced SpeedStep Technology (EIST, ESS)");
    add(cpu.fn_1_ecx.has_tm2, "Thermal Monitor 2 (TM2)");
    add(cpu.fn_1_ecx.has_ssse3, "Supplemental Streaming SIMD Extension 3 (SSSE3)");
    add(cpu.fn_1_ecx.has_cnxt_id, "L1 Context ID");
    add(cpu.fn_1_ecx.has_sdbg, "Silicon Debug Interface");
    add(cpu.fn_1_ecx.has_fma, "Fused Multiply Add (FMA)");
    add(cpu.fn_1_ecx.has_cx16, "CMPXCHG16B Instruction");
    add(cpu.fn_1_ecx.has_xtpr, "xTPR Update Control");
    add(cpu.fn_1_ecx.has_pdcm, "Perfmon and Debug Capability");
    add(cpu.fn_1_ecx.has_pcid, "Process Context Identifiers");
    add(cpu.fn_1_ecx.has_dca, "Direct Cache Access (DCA)");
    add(cpu.fn_1_ecx.has_sse41, "Streaming SIMD Extension 4.1 (SSE4.1)");
    add(cpu.fn_1_ecx.has_sse42, "Streaming SIMD Extension 4.2 (SSE4.2)");
    add(cpu.fn_1_ecx.has_x2apic, "Extended xAPIC Support");
    add(cpu.fn_1_ecx.has_movbe, "MOVBE Instruction");
    add(cpu.fn_1_ecx.has_popcnt, "POPCNT Instruction");
    add(cpu.fn_1_ecx.has_tsc_deadline, "Time Stamp Counter Deadline");
    add(cpu.fn_1_ecx.has_aes, "AES Instruction (AES)");
    add(cpu.fn_1_ecx.has_xsave, "XSAVE/XSTOR States");
    add(cpu.fn_1_ecx.has_osxsave, "OS-Enabled Extended State Management");
    add(cpu.fn_1_ecx.has_avx, "Advanced Vector Extension (AVX)");
    add(cpu.fn_1_ecx.has_f16c, "Float-16-bit Convertsion Instructions (F16C)");
    add(cpu.fn_1_ecx.has_rdrand, "RDRAND Instruction");
    add(cpu.fn_1_ecx.has_hypervisor, "Hypervisor");

    // Function 0x80000001 EDX
    add(cpu.fn_81_edx.has_syscall, "SYSCALL / SYSRET Instructions");
    add(cpu.fn_81_edx.has_xd_bit, "Execution Disable Bit");
    add(cpu.fn_81_edx.has_mmxext, "AMD Extended MMX");
    add(cpu.fn_81_edx.has_1gb_pages, "1 GB Page Size");
    add(cpu.fn_81_edx.has_rdtscp, "RDTSCP Instruction");
    add(cpu.fn_81_edx.has_intel64, "64-bit x86 Extension (AMD64, Intel64)");
    add(cpu.fn_81_edx.has_3dnowext, "AMD Extended 3DNow!");
    add(cpu.fn_81_edx.has_3dnow, "AMD 3DNow!");

    // Function 0x80000001 ECX
    add(cpu.fn_81_ecx.has_lahf, "LAHF/SAHF Instructions");
    add(cpu.fn_81_ecx.has_svm, "Secure Virtual Machine (SVM, Pacifica)");
    add(cpu.fn_81_ecx.has_lzcnt, "LZCNT Instruction");
    add(cpu.fn_81_ecx.has_sse4a, "AMD SSE4A");
    add(cpu.fn_81_ecx.has_misalignsse, "AMD MisAligned SSE");
    add(cpu.fn_81_ecx.has_3dnow_prefetch, "AMD 3DNowPrefetch");
    add(cpu.fn_81_ecx.has_xop, "AMD XOP");
    add(cpu.fn_81_ecx.has_wdt, "Watchdog Timer");
    add(cpu.fn_81_ecx.has_fma4, "AMD FMA4");

    // Function 7 EBX
    add(cpu.fn_7_0_ebx.has_fsgsbase, "RDFSBASE / RDGSBASE / WRFSBASE / WRGSBASE Instruction");
    add(cpu.fn_7_0_ebx.has_sgx, "Software Guard Extensions (SGE)");
    add(cpu.fn_7_0_ebx.has_bmi1, "Bit Manipulation Instruction Set 1 (BMI1)");
    add(cpu.fn_7_0_ebx.has_hle, "Transactional Synchronization Extensions");
    add(cpu.fn_7_0_ebx.has_avx2, "Advanced Vector Extensions 2 (AVX2)");
    add(cpu.fn_7_0_ebx.has_smep, "Supervisor-Mode Execution Prevention");
    add(cpu.fn_7_0_ebx.has_bmi2, "Bit Manipulation Instruction Set 2 (BMI2)");
    add(cpu.fn_7_0_ebx.has_erms, "Enhanced REP MOVSB/STOSB");
    add(cpu.fn_7_0_ebx.has_invpcid, "INVPCID Instruction");
    add(cpu.fn_7_0_ebx.has_rtm, "Transactional Synchronization Extensions (RTM)");
    add(cpu.fn_7_0_ebx.has_pqm, "Platform Quality of Service Monitoring (PQM)");
    add(cpu.fn_7_0_ebx.has_mpx, "Memory Protection Extensions (MPX)");
    add(cpu.fn_7_0_ebx.has_pqe, "Platform Quality of Service Enforcement (PQE)");
    add(cpu.fn_7_0_ebx.has_avx512f, "AVX-512 Foundation (AVX512F)");
    add(cpu.fn_7_0_ebx.has_avx512dq, "AVX-512 Doubleword and Quadword Instructions (AVX512DQ)");
    add(cpu.fn_7_0_ebx.has_rdseed, "RDSEED Instruction");
    add(cpu.fn_7_0_ebx.has_adx, "Multi-Precision Add-Carry Instruction Extensions (ADX)");
    add(cpu.fn_7_0_ebx.has_smap, "Supervisor Mode Access Prevention (SMAP)");
    add(cpu.fn_7_0_ebx.has_avx512ifma, "AVX-512 Integer Fused Multiply-Add Instructions");
    add(cpu.fn_7_0_ebx.has_pcommit, "PCOMMIT Instruction");
    add(cpu.fn_7_0_ebx.has_clflushopt, "CLFLUSHOPT Instruction");
    add(cpu.fn_7_0_ebx.has_clwb, "CLWB Instruction");
    add(cpu.fn_7_0_ebx.has_intel_pt, "Intel Processor Trace");
    add(cpu.fn_7_0_ebx.has_avx512pf, "AVX-512 Prefetch Instructions (AVX512PF)");
    add(cpu.fn_7_0_ebx.has_avx512er, "AVX-512 Exponential and Reciprocal Instructions (AVX512ER)");
    add(cpu.fn_7_0_ebx.has_avx512cd, "AVX-512 Conflict Detection Instructions (AVX512CD)");
    add(cpu.fn_7_0_ebx.has_sha, "Intel SHA Extensions (SHA)");
    add(cpu.fn_7_0_ebx.has_avx512bw, "AVX-512 Byte and Word Instructions (AVX512BW)");
    add(cpu.fn_7_0_ebx.has_avx512vl, "AVX-512 Vector Length Extensions (AVX512VL)");

    // Function 7 ECX
    add(cpu.fn_7_0_ecx.has_prefetchwt1, "PREFETCHWT1 Instruction");
    add(cpu.fn_7_0_ecx.has_avx512vbmi, "AVX-512 Vector Bit Manipulation Instructions (AVX512VBMI)");
    add(cpu.fn_7_0_ecx.has_umip, "User-mode Instruction Prevention (UMIP)");
    add(cpu.fn_7_0_ecx.has_pku, "Memory Protection Keys for User-mode pages (PKU)");
    add(cpu.fn_7_0_ecx.has_ospke, "PKU enabled by OS (OSPKE)");
    add(cpu.fn_7_0_ecx.has_avx512vbmi2, "AVX-512 Vector Bit Manipulation Instructions 2 (AVX512VBMI2)");
    add(cpu.fn_7_0_ecx.has_gfni, "Galois Field Instructions (GFNI)");
    add(cpu.fn_7_0_ecx.has_vaes, "AES Instruction Set (VEX-256/EVEX)");
    add(cpu.fn_7_0_ecx.has_vpclmulqdq, "CLMUL Instruction Set (VEX-256/EVEX)");
    add(cpu.fn_7_0_ecx.has_avx512vnni, "AVX-512 Vector Neural Network Instructions (AVX512VNNI)");
    add(cpu.fn_7_0_ecx.has_avx512bitalg, "AVX-512 BITALG Instructions (AVX512BITALG)");
    add(cpu.fn_7_0_ecx.has_avx512vpopcntdq, "AVX-512 Vector Population Count D/Q (AVX512VPOPCNTDQ)");
    add(cpu.fn_7_0_ecx.has_rdpid, "Read Processor ID (RDPID)");
    add(cpu.fn_7_0_ecx.has_sgx_lc, "SGX Launch Configuration");

    // Function 7 EDX
    add(cpu.fn_7_0_edx.has_avx512_4vnniw, "AVX-512 4-register Neural Network Instructions (AVX5124VNNIW)");
    add(cpu.fn_7_0_edx.has_avx512_4fmaps, "AVX-512 4-register Multiply Accumulation Single precision (AVX5124FMAPS)");

    // Function 0xC0000001 EDX
    add(cpu.fn_c1_edx.has_ais, "VIA Alternate Instruction Set");
    add(cpu.fn_c1_edx.has_rng, "Hardware Random Number Generator (RNG)");
    add(cpu.fn_c1_edx.has_lh, "LongHaul MSR 0000_110Ah");
    add(cpu.fn_c1_edx.has_femms, "VIA FEMMS Instruction");
    add(cpu.fn_c1_edx.has_ace, "Advanced Cryptography Engine (ACE)");
    add(cpu.fn_c1_edx.has_ace2, "Advanced Cryptography Engine 2 (ACE2)");
    add(cpu.fn_c1_edx.has_phe, "PadLock Hash Engine (PHE)");
    add(cpu.fn_c1_edx.has_pmm, "PadLock Montgomery Multiplier (PMM)");
    add(cpu.fn_c1_edx.has_parallax, "Parallax");
    add(cpu.fn_c1_edx.has_overstress, "Overstress");
    add(cpu.fn_c1_edx.has_tm3, "Thermal Monitor 3 (TM3)");
    add(cpu.fn_c1_edx.has_rng2, "Hardware Random Number Generator 2 (RNG2)");
    add(cpu.fn_c1_edx.has_phe2, "PadLock Hash Engine 2 (PHE2)");

    std::sort(features.begin(), features.end());

    for (const auto& feature : features)
    {
        proto::CentralProcessor::Feature* item = message.add_feature();
        item->set_name(feature.first);
        item->set_supported(feature.second);
    }

    return message.SerializeAsString();
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

void CategoryOpticalDrives::Parse(Output* /* output */, const std::string& /* data */)
{
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

void CategoryATA::Parse(Output* output, const std::string& data)
{
    proto::AtaDrives message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::AtaDrives::Item& item = message.item(index);

        Output::Group group(output, item.model_number(), Icon());

        if (!item.serial_number().empty())
            output->AddParam(IDI_DRIVE, "Serial Number", item.serial_number());

        if (!item.firmware_revision().empty())
            output->AddParam(IDI_DRIVE, "Firmware Revision", item.firmware_revision());

        if (!item.bus_type().empty())
            output->AddParam(IDI_DRIVE, "Bus Type", item.bus_type());

        if (!item.transfer_mode().empty())
            output->AddParam(IDI_DRIVE, "Transfer Mode", item.transfer_mode());

        if (item.rotation_rate())
            output->AddParam(IDI_DRIVE, "Rotation Rate", std::to_string(item.rotation_rate()), "RPM");

        if (item.drive_size())
            output->AddParam(IDI_DRIVE, "Drive Size", std::to_string(item.drive_size()), "Bytes");

        if (item.buffer_size())
            output->AddParam(IDI_DRIVE, "Buffer Size", std::to_string(item.buffer_size()), "Bytes");

        if (item.multisectors())
            output->AddParam(IDI_DRIVE, "Multisectors", std::to_string(item.multisectors()));

        if (item.ecc_size())
            output->AddParam(IDI_DRIVE, "ECC Size", std::to_string(item.ecc_size()));

        output->AddParam(IDI_DRIVE, "Removable", item.is_removable() ? "Yes" : "No");

        if (item.heads_number())
            output->AddParam(IDI_DRIVE, "Heads Count", std::to_string(item.heads_number()));

        if (item.cylinders_number())
            output->AddParam(IDI_DRIVE, "Cylinders Count", std::to_string(item.cylinders_number()));

        if (item.tracks_per_cylinder())
            output->AddParam(IDI_DRIVE, "Tracks per Cylinder", std::to_string(item.tracks_per_cylinder()));

        if (item.sectors_per_track())
            output->AddParam(IDI_DRIVE, "Sectors per Track", std::to_string(item.sectors_per_track()));

        if (item.bytes_per_sector())
            output->AddParam(IDI_DRIVE, "Bytes per Sector", std::to_string(item.bytes_per_sector()));

        if (item.feature_size())
        {
            Output::Group features_group(output, "Features", Icon());

            for (int i = 0; i < item.feature_size(); ++i)
            {
                output->AddParam(item.feature(i).enabled() ? IDI_CHECKED : IDI_UNCHECKED,
                                 item.feature(i).name(),
                                 item.feature(i).enabled() ? "Yes" : "No");
            }
        }
    }
}

std::string CategoryATA::Serialize()
{
    proto::AtaDrives message;

    for (PhysicalDriveEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::AtaDrives::Item* item = message.add_item();

        item->set_model_number(enumerator.GetModelNumber());
        item->set_serial_number(enumerator.GetSerialNumber());
        item->set_firmware_revision(enumerator.GetFirmwareRevision());
        item->set_bus_type(enumerator.GetBusType());
        item->set_transfer_mode(enumerator.GetCurrentTransferMode());
        item->set_rotation_rate(enumerator.GetRotationRate());
        item->set_drive_size(enumerator.GetDriveSize());
        item->set_buffer_size(enumerator.GetBufferSize());
        item->set_multisectors(enumerator.GetMultisectors());
        item->set_ecc_size(enumerator.GetECCSize());
        item->set_is_removable(enumerator.IsRemovable());
        item->set_cylinders_number(enumerator.GetCylindersNumber());
        item->set_tracks_per_cylinder(enumerator.GetTracksPerCylinder());
        item->set_sectors_per_track(enumerator.GetSectorsPerTrack());
        item->set_bytes_per_sector(enumerator.GetBytesPerSector());
        item->set_heads_number(enumerator.GetHeadsNumber());

        for (const auto& feature : enumerator.GetFeatures())
        {
            auto feature_item = item->add_feature();
            feature_item->set_name(feature.first);
            feature_item->set_enabled(feature.second);
        }
    }

    return message.SerializeAsString();
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

void CategorySMART::Parse(Output* /* output */, const std::string& /* data */)
{
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
// CategoryVideoAdapters
//

const char* CategoryVideoAdapters::Name() const
{
    return "Video Adapters";
}

Category::IconId CategoryVideoAdapters::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryVideoAdapters::Guid() const
{
    return "09E9069D-C394-4CD7-8252-E5CF83B7674C";
}

void CategoryVideoAdapters::Parse(Output* output, const std::string& data)
{
    proto::VideoAdapters message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::VideoAdapters::Item& item = message.item(index);

        Output::Group group(output, item.description(), Icon());

        output->AddParam(IDI_MONITOR, "Description", item.description());
        output->AddParam(IDI_MONITOR, "Adapter String", item.adapter_string());
        output->AddParam(IDI_MONITOR, "BIOS String", item.bios_string());
        output->AddParam(IDI_MONITOR, "Chip Type", item.chip_type());
        output->AddParam(IDI_MONITOR, "DAC Type", item.dac_type());
        output->AddParam(IDI_MONITOR, "Memory Size", std::to_string(item.memory_size()), "Bytes");
        output->AddParam(IDI_MONITOR, "Driver Date", item.driver_date());
        output->AddParam(IDI_MONITOR, "Driver Version", item.driver_version());
        output->AddParam(IDI_MONITOR, "Driver Provider", item.driver_provider());
    }
}

std::string CategoryVideoAdapters::Serialize()
{
    proto::VideoAdapters message;

    for (VideoAdapterEnumarator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::VideoAdapters::Item* item = message.add_item();

        item->set_description(enumerator.GetDescription());
        item->set_adapter_string(enumerator.GetAdapterString());
        item->set_bios_string(enumerator.GetBIOSString());
        item->set_chip_type(enumerator.GetChipString());
        item->set_dac_type(enumerator.GetDACType());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_provider(enumerator.GetDriverVendor());
        item->set_memory_size(enumerator.GetMemorySize());
    }

    return message.SerializeAsString();
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

void CategoryMonitor::Parse(Output* output, const std::string& data)
{
    proto::Monitors message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Monitors::Item& item = message.item(index);

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

        if (!item.input_signal_type().empty())
            output->AddParam(IDI_MONITOR, "Input Signal Type", item.input_signal_type());

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
                const proto::Monitors::Item::Timing& timing = item.timings(mode);

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
    proto::Monitors message;

    for (MonitorEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        std::unique_ptr<Edid> edid = enumerator.GetEDID();
        if (!edid)
            continue;

        proto::Monitors::Item* item = message.add_item();

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
        item->set_input_signal_type(edid->GetInputSignalType());

        uint8_t supported_features = edid->GetFeatureSupport();

        if (supported_features & Edid::FEATURE_SUPPORT_DEFAULT_GTF_SUPPORTED)
            item->set_default_gtf_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SUSPEND)
            item->set_suspend_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_STANDBY)
            item->set_standby_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_ACTIVE_OFF)
            item->set_active_off_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_PREFERRED_TIMING_MODE)
            item->set_preferred_timing_mode_supported(true);

        if (supported_features & Edid::FEATURE_SUPPORT_SRGB)
            item->set_srgb_supported(true);

        auto add_timing = [&](int width, int height, int freq)
        {
            proto::Monitors::Item::Timing* timing = item->add_timings();

            timing->set_width(width);
            timing->set_height(height);
            timing->set_frequency(freq);
        };

        uint8_t estabilished_timings1 = edid->GetEstabilishedTimings1();

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

        uint8_t estabilished_timings2 = edid->GetEstabilishedTimings2();

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

        uint8_t manufacturer_timings = edid->GetManufacturersTimings();

        if (manufacturer_timings & Edid::MANUFACTURERS_TIMINGS_1152X870_75HZ)
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

void CategoryPrinters::Parse(Output* output, const std::string& data)
{
    proto::Printers message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Printers::Item& item = message.item(index);

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
            case proto::Printers::Item::ORIENTATION_LANDSCAPE:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Landscape");
                break;

            case proto::Printers::Item::ORIENTATION_PORTRAIT:
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
    proto::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Printers::Item* item = message.add_item();

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
                item->set_orientation(proto::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(proto::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(proto::Printers::Item::ORIENTATION_UNKNOWN);
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

void CategoryPowerOptions::Parse(Output* output, const std::string& data)
{
    proto::PowerOptions message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::PARAM_VALUE);

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    const char* power_source;
    switch (message.power_source())
    {
        case proto::PowerOptions::POWER_SOURCE_DC_BATTERY:
            power_source = "DC Battery";
            break;

        case proto::PowerOptions::POWER_SOURCE_AC_LINE:
            power_source = "AC Line";
            break;

        default:
            power_source = "Unknown";
            break;
    }

    output->AddParam(IDI_POWER_SUPPLY, "Power Source", power_source);

    const char* battery_status;
    switch (message.battery_status())
    {
        case proto::PowerOptions::BATTERY_STATUS_HIGH:
            battery_status = "High";
            break;

        case proto::PowerOptions::BATTERY_STATUS_LOW:
            battery_status = "Low";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CRITICAL:
            battery_status = "Critical";
            break;

        case proto::PowerOptions::BATTERY_STATUS_CHARGING:
            battery_status = "Charging";
            break;

        case proto::PowerOptions::BATTERY_STATUS_NO_BATTERY:
            battery_status = "No Battery";
            break;

        default:
            battery_status = "Unknown";
            break;
    }

    output->AddParam(IDI_BATTERY, "Battery Status", battery_status);

    if (message.battery_status() != proto::PowerOptions::BATTERY_STATUS_NO_BATTERY &&
        message.battery_status() != proto::PowerOptions::BATTERY_STATUS_UNKNOWN)
    {
        output->AddParam(IDI_BATTERY,
                         "Battery Life Percent",
                         std::to_string(message.battery_life_percent()),
                         "%%");

        output->AddParam(IDI_BATTERY,
                         "Full Battery Life Time",
                         std::to_string(message.full_battery_life_time()),
                         "s");

        output->AddParam(IDI_BATTERY,
                         "Remaining Battery Life Time",
                         std::to_string(message.remaining_battery_life_time()),
                         "s");
    }

    for (int index = 0; index < message.battery_size(); ++index)
    {
        const proto::PowerOptions::Battery& battery = message.battery(index);

        Output::Group group(output, StringPrintf("Battery #%d", index + 1), IDI_BATTERY);

        if (!battery.device_name().empty())
            output->AddParam(IDI_BATTERY, "Device Name", battery.device_name());

        if (!battery.manufacturer().empty())
            output->AddParam(IDI_BATTERY, "Manufacturer", battery.manufacturer());

        if (!battery.manufacture_date().empty())
            output->AddParam(IDI_BATTERY, "Manufacture Date", battery.manufacture_date());

        if (!battery.unique_id().empty())
            output->AddParam(IDI_BATTERY, "Unique Id", battery.unique_id());

        if (!battery.serial_number().empty())
            output->AddParam(IDI_BATTERY, "Serial Number", battery.serial_number());

        if (!battery.temperature().empty())
            output->AddParam(IDI_BATTERY, "Tempareture", battery.temperature());

        if (battery.design_capacity() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Design Capacity",
                             std::to_string(battery.design_capacity()),
                             "mWh");
        }

        if (!battery.type().empty())
            output->AddParam(IDI_BATTERY, "Type", battery.type());

        if (battery.full_charged_capacity() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Full Charged Capacity",
                             std::to_string(battery.full_charged_capacity()),
                             "mWh");
        }

        if (battery.depreciation() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Depreciation",
                             std::to_string(battery.depreciation()),
                             "%%");
        }

        if (battery.current_capacity() != 0)
        {
            output->AddParam(IDI_BATTERY,
                             "Current Capacity",
                             std::to_string(battery.current_capacity()),
                             "mWh");
        }

        if (battery.voltage() != 0)
            output->AddParam(IDI_BATTERY, "Voltage", std::to_string(battery.voltage()), "mV");

        if (battery.state() != 0)
        {
            Output::Group state_group(output, "State", IDI_BATTERY);

            if (battery.state() & proto::PowerOptions::Battery::STATE_CHARGING)
                output->AddParam(IDI_CHECKED, "Charging", "Yes");

            if (battery.state() & proto::PowerOptions::Battery::STATE_CRITICAL)
                output->AddParam(IDI_CHECKED, "Critical", "Yes");

            if (battery.state() & proto::PowerOptions::Battery::STATE_DISCHARGING)
                output->AddParam(IDI_CHECKED, "Discharging", "Yes");

            if (battery.state() & proto::PowerOptions::Battery::STATE_POWER_ONLINE)
                output->AddParam(IDI_CHECKED, "Power OnLine", "Yes");
        }
    }
}

std::string CategoryPowerOptions::Serialize()
{
    proto::PowerOptions message;

    SYSTEM_POWER_STATUS power_status;
    memset(&power_status, 0, sizeof(power_status));

    if (GetSystemPowerStatus(&power_status))
    {
        switch (power_status.ACLineStatus)
        {
            case 0:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_DC_BATTERY);
                break;

            case 1:
                message.set_power_source(proto::PowerOptions::POWER_SOURCE_AC_LINE);
                break;

            default:
                break;
        }

        switch (power_status.BatteryFlag)
        {
            case 1:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_HIGH);
                break;

            case 2:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_LOW);
                break;

            case 4:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CRITICAL);
                break;

            case 8:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_CHARGING);
                break;

            case 128:
                message.set_battery_status(proto::PowerOptions::BATTERY_STATUS_NO_BATTERY);
                break;

            default:
                break;
        }

        if (power_status.BatteryFlag != 128)
        {
            message.set_battery_life_percent(power_status.BatteryLifePercent);

            if (power_status.BatteryFullLifeTime != -1)
                message.set_full_battery_life_time(power_status.BatteryFullLifeTime);

            if (power_status.BatteryLifeTime != -1)
                message.set_remaining_battery_life_time(power_status.BatteryLifeTime);
        }
    }

    for (BatteryEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::PowerOptions::Battery* battery = message.add_battery();

        battery->set_device_name(enumerator.GetDeviceName());
        battery->set_manufacturer(enumerator.GetManufacturer());
        battery->set_manufacture_date(enumerator.GetManufactureDate());
        battery->set_unique_id(enumerator.GetUniqueId());
        battery->set_serial_number(enumerator.GetSerialNumber());
        battery->set_temperature(enumerator.GetTemperature());
        battery->set_design_capacity(enumerator.GetDesignCapacity());
        battery->set_type(enumerator.GetType());
        battery->set_full_charged_capacity(enumerator.GetFullChargedCapacity());
        battery->set_depreciation(enumerator.GetDepreciation());
        battery->set_current_capacity(enumerator.GetCurrentCapacity());
        battery->set_voltage(enumerator.GetVoltage());

        const uint32_t state = enumerator.GetState();

        if (state & BatteryEnumerator::STATE_CHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CHARGING);
        }

        if (state & BatteryEnumerator::STATE_CRITICAL)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_CRITICAL);
        }

        if (state & BatteryEnumerator::STATE_DISCHARGING)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_DISCHARGING);
        }

        if (state & BatteryEnumerator::STATE_POWER_ONLINE)
        {
            battery->set_state(
                battery->state() | proto::PowerOptions::Battery::STATE_POWER_ONLINE);
        }
    }

    return message.SerializeAsString();
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

void CategoryWindowsDevices::Parse(Output* output, const std::string& data)
{
    proto::WindowsDevices message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name(), Output::TableType::LIST);

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
        const proto::WindowsDevices::Item& item = message.item(index);

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
    proto::WindowsDevices message;

    for (DeviceEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::WindowsDevices::Item* item = message.add_item();

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
