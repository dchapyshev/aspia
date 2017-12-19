//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_pointing_device.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "system_info/category_dmi_pointing_device.h"
#include "system_info/category_dmi_pointing_device.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiPointingDevice::Type value)
{
    switch (value)
    {
        case proto::DmiPointingDevice::TYPE_OTHER:
            return "Other";

        case proto::DmiPointingDevice::TYPE_MOUSE:
            return "Mouse";

        case proto::DmiPointingDevice::TYPE_TRACK_BALL:
            return "Track Ball";

        case proto::DmiPointingDevice::TYPE_TRACK_POINT:
            return "Track Point";

        case proto::DmiPointingDevice::TYPE_GLIDE_POINT:
            return "Glide Point";

        case proto::DmiPointingDevice::TYPE_TOUCH_PAD:
            return "Touch Pad";

        case proto::DmiPointingDevice::TYPE_TOUCH_SCREEN:
            return "Touch Screen";

        case proto::DmiPointingDevice::TYPE_OPTICAL_SENSOR:
            return "Optical Sensor";

        default:
            return "Unknown";
    }
}

const char* InterfaceToString(proto::DmiPointingDevice::Interface value)
{
    switch (value)
    {
        case proto::DmiPointingDevice::INTERFACE_OTHER:
            return "Other";

        case proto::DmiPointingDevice::INTERFACE_SERIAL:
            return "Serial";

        case proto::DmiPointingDevice::INTERFACE_PS_2:
            return "PS/2";

        case proto::DmiPointingDevice::INTERFACE_INFRARED:
            return "Infrared";

        case proto::DmiPointingDevice::INTERFACE_HP_HIL:
            return "HP-HIL";

        case proto::DmiPointingDevice::INTERFACE_BUS_MOUSE:
            return "Bus mouse";

        case proto::DmiPointingDevice::INTERFACE_ADB:
            return "ADB (Apple Desktop Bus)";

        case proto::DmiPointingDevice::INTERFACE_BUS_MOUSE_DB_9:
            return "Bus mouse DB-9";

        case proto::DmiPointingDevice::INTERFACE_BUS_MOUSE_MICRO_DIN:
            return "Bus mouse micro-DIN";

        case proto::DmiPointingDevice::INTERFACE_USB:
            return "USB";

        default:
            return "Unknown";
    }
}

proto::DmiPointingDevice::Type GetDeviceType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiPointingDevice::TYPE_OTHER;
        case 0x02: return proto::DmiPointingDevice::TYPE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevice::TYPE_MOUSE;
        case 0x04: return proto::DmiPointingDevice::TYPE_TRACK_BALL;
        case 0x05: return proto::DmiPointingDevice::TYPE_TRACK_POINT;
        case 0x06: return proto::DmiPointingDevice::TYPE_GLIDE_POINT;
        case 0x07: return proto::DmiPointingDevice::TYPE_TOUCH_PAD;
        case 0x08: return proto::DmiPointingDevice::TYPE_TOUCH_SCREEN;
        case 0x09: return proto::DmiPointingDevice::TYPE_OPTICAL_SENSOR;
        default: return proto::DmiPointingDevice::TYPE_UNKNOWN;
    }
}

proto::DmiPointingDevice::Interface GetInterface(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiPointingDevice::INTERFACE_OTHER;
        case 0x02: return proto::DmiPointingDevice::INTERFACE_UNKNOWN;
        case 0x03: return proto::DmiPointingDevice::INTERFACE_SERIAL;
        case 0x04: return proto::DmiPointingDevice::INTERFACE_PS_2;
        case 0x05: return proto::DmiPointingDevice::INTERFACE_INFRARED;
        case 0x06: return proto::DmiPointingDevice::INTERFACE_HP_HIL;
        case 0x07: return proto::DmiPointingDevice::INTERFACE_BUS_MOUSE;
        case 0x08: return proto::DmiPointingDevice::INTERFACE_ADB;
        case 0xA0: return proto::DmiPointingDevice::INTERFACE_BUS_MOUSE_DB_9;
        case 0xA1: return proto::DmiPointingDevice::INTERFACE_BUS_MOUSE_MICRO_DIN;
        case 0xA2: return proto::DmiPointingDevice::INTERFACE_USB;
        default: return proto::DmiPointingDevice::INTERFACE_UNKNOWN;
    }
}

} // namespace

CategoryDmiPointingDevice::CategoryDmiPointingDevice()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiPointingDevice::Name() const
{
    return "Pointing Devices";
}

Category::IconId CategoryDmiPointingDevice::Icon() const
{
    return IDI_MOUSE;
}

const char* CategoryDmiPointingDevice::Guid() const
{
    return "{6883684B-3CEC-451B-A2E3-34C16348BA1B}";
}

void CategoryDmiPointingDevice::Parse(Table& table, const std::string& data)
{
    proto::DmiPointingDevice message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPointingDevice::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Build-in Pointing Device #%d", index + 1));

        group.AddParam("Device Type", Value::String(TypeToString(item.device_type())));
        group.AddParam("Device Interface", Value::String(InterfaceToString(item.device_interface())));
        group.AddParam("Buttons Count", Value::Number(item.button_count()));
    }
}

std::string CategoryDmiPointingDevice::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPointingDevice message;

    for (SMBios::TableEnumeratorNew table_enumerator(*smbios, SMBios::TABLE_TYPE_POINTING_DEVICE);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::TableReader table = table_enumerator.GetTable();
        if (table.GetTableLength() < 0x07)
            continue;

        proto::DmiPointingDevice::Item* item = message.add_item();

        item->set_device_type(GetDeviceType(table.GetByte(0x04)));
        item->set_device_interface(GetInterface(table.GetByte(0x05)));
        item->set_button_count(table.GetByte(0x06));
    }

    return message.SerializeAsString();
}

} // namespace aspia
