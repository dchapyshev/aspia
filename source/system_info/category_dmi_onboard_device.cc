//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_onboard_device.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/bitset.h"
#include "system_info/category_dmi_onboard_device.h"
#include "system_info/category_dmi_onboard_device.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiOnBoardDevice::Type value)
{
    switch (value)
    {
        case proto::DmiOnBoardDevice::TYPE_OTHER:
            return "Other";

        case proto::DmiOnBoardDevice::TYPE_VIDEO:
            return "Video";

        case proto::DmiOnBoardDevice::TYPE_SCSI_CONTROLLER:
            return "SCSI Controller";

        case proto::DmiOnBoardDevice::TYPE_ETHERNET:
            return "Ethernet";

        case proto::DmiOnBoardDevice::TYPE_TOKEN_RING:
            return "Token Ring";

        case proto::DmiOnBoardDevice::TYPE_SOUND:
            return "Sound";

        case proto::DmiOnBoardDevice::TYPE_PATA_CONTROLLER:
            return "PATA Controller";

        case proto::DmiOnBoardDevice::TYPE_SATA_CONTROLLER:
            return "SATA Controller";

        case proto::DmiOnBoardDevice::TYPE_SAS_CONTROLLER:
            return "SAS Controller";

        default:
            return "Unknown";
    }
}

std::string GetDescription(const SMBios::TableReader& table, uint8_t handle)
{
    if (!handle)
        return std::string();

    char* string = reinterpret_cast<char*>(const_cast<uint8_t*>(
        table.GetPointer(0))) + table.GetTableLength();

    while (handle > 1 && *string)
    {
        string += strlen(string);
        ++string;
        --handle;
    }

    if (!*string || !string[0])
        return std::string();

    std::string output;
    TrimWhitespaceASCII(string, TRIM_ALL, output);
    return output;
}

proto::DmiOnBoardDevice::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiOnBoardDevice::TYPE_OTHER;
        case 0x02: return proto::DmiOnBoardDevice::TYPE_UNKNOWN;
        case 0x03: return proto::DmiOnBoardDevice::TYPE_VIDEO;
        case 0x04: return proto::DmiOnBoardDevice::TYPE_SCSI_CONTROLLER;
        case 0x05: return proto::DmiOnBoardDevice::TYPE_ETHERNET;
        case 0x06: return proto::DmiOnBoardDevice::TYPE_TOKEN_RING;
        case 0x07: return proto::DmiOnBoardDevice::TYPE_SOUND;
        case 0x08: return proto::DmiOnBoardDevice::TYPE_PATA_CONTROLLER;
        case 0x09: return proto::DmiOnBoardDevice::TYPE_SATA_CONTROLLER;
        case 0x0A: return proto::DmiOnBoardDevice::TYPE_SAS_CONTROLLER;
        default: return proto::DmiOnBoardDevice::TYPE_UNKNOWN;
    }
}

} // namespace

CategoryDmiOnboardDevice::CategoryDmiOnboardDevice()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiOnboardDevice::Name() const
{
    return "Onboard Devices";
}

Category::IconId CategoryDmiOnboardDevice::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiOnboardDevice::Guid() const
{
    return "{6C62195C-5E5F-41BA-B6AD-99041594DAC6}";
}

void CategoryDmiOnboardDevice::Parse(Table& table, const std::string& data)
{
    proto::DmiOnBoardDevice message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiOnBoardDevice::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("OnBoard Device #%d", index + 1));

        if (!item.description().empty())
            group.AddParam("Description", Value::String(item.description()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Status", Value::String(item.enabled() ? "Enabled" : "Disabled"));
    }
}

std::string CategoryDmiOnboardDevice::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiOnBoardDevice message;

    for (SMBios::TableEnumeratorNew table_enumerator(*smbios, SMBios::TABLE_TYPE_ONBOARD_DEVICE);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::TableReader table = table_enumerator.GetTable();

        uint8_t count = (table.GetTableLength() - 4) / 2;
        const uint8_t* ptr = table.GetPointer(0) + 4;

        for (uint8_t index = 0; index < count; ++index)
        {
            proto::DmiOnBoardDevice::Item* item = message.add_item();

            item->set_description(GetDescription(table, ptr[2 * index + 1]));

            BitSet<uint8_t> flags = ptr[2 * index];

            item->set_type(GetType(flags.Range(0, 6)));
            item->set_enabled(flags.Test(7));
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
