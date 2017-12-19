//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_system_slot.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "system_info/category_dmi_system_slot.h"
#include "system_info/category_dmi_system_slot.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiSystemSlot::Type value)
{
    switch (value)
    {
        case proto::DmiSystemSlot::TYPE_OTHER:
            return "Other";

        case proto::DmiSystemSlot::TYPE_ISA:
            return "ISA";

        case proto::DmiSystemSlot::TYPE_MCA:
            return "MCA";

        case proto::DmiSystemSlot::TYPE_EISA:
            return "EISA";

        case proto::DmiSystemSlot::TYPE_PCI:
            return "PCI";

        case proto::DmiSystemSlot::TYPE_PC_CARD:
            return "PC Card (PCMCIA)";

        case proto::DmiSystemSlot::TYPE_VLB:
            return "VLB";

        case proto::DmiSystemSlot::TYPE_PROPRIETARY:
            return "Proprietary";

        case proto::DmiSystemSlot::TYPE_PROCESSOR_CARD:
            return "Processor Card";

        case proto::DmiSystemSlot::TYPE_PROPRIETARY_MEMORY_CARD:
            return "Proprietary Memory Card";

        case proto::DmiSystemSlot::TYPE_IO_RISER_CARD:
            return "I/O Riser Card";

        case proto::DmiSystemSlot::TYPE_NUBUS:
            return "NuBus";

        case proto::DmiSystemSlot::TYPE_PCI_66:
            return "PCI-66";

        case proto::DmiSystemSlot::TYPE_AGP:
            return "AGP";

        case proto::DmiSystemSlot::TYPE_AGP_2X:
            return "AGP 2x";

        case proto::DmiSystemSlot::TYPE_AGP_4X:
            return "AGP 4x";

        case proto::DmiSystemSlot::TYPE_PCI_X:
            return "PCI-X";

        case proto::DmiSystemSlot::TYPE_AGP_8X:
            return "AGP 8x";

        case proto::DmiSystemSlot::TYPE_M2_SOCKET_1DP:
            return "M.2 Socket 1-DP";

        case proto::DmiSystemSlot::TYPE_M2_SOCKET_1SD:
            return "M.2 Socket 1-SD";

        case proto::DmiSystemSlot::TYPE_M2_SOCKET_2:
            return "M.2 Socket 2";

        case proto::DmiSystemSlot::TYPE_M2_SOCKET_3:
            return "M.2 Socket 3";

        case proto::DmiSystemSlot::TYPE_MXM_TYPE_I:
            return "MXM Type I";

        case proto::DmiSystemSlot::TYPE_MXM_TYPE_II:
            return "MXM Type II";

        case proto::DmiSystemSlot::TYPE_MXM_TYPE_III:
            return "MXM Type III";

        case proto::DmiSystemSlot::TYPE_MXM_TYPE_III_HE:
            return "MXM Type III-HE";

        case proto::DmiSystemSlot::TYPE_MXM_TYPE_IV:
            return "MXM Type IV";

        case proto::DmiSystemSlot::TYPE_MXM_30_TYPE_A:
            return "MXM 3.0 Type A";

        case proto::DmiSystemSlot::TYPE_MXM_30_TYPE_B:
            return "MXM 3.0 Type B";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_SFF_8639:
            return "PCI Express 2 SFF-8639";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_SFF_8639:
            return "PCI Express 3 SFF-8639";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_52PIN_WITH_BOTTOM_SIDE:
            return "PCI Express Mini 52-pin with bottom-side keep-outs";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_52PIN:
            return "PCI Express Mini 52-pin without bottom-side keep-outs";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_76PIN:
            return "PCI Express Mini 76-pin";

        case proto::DmiSystemSlot::TYPE_PC98_C20:
            return "PC-98/C20";

        case proto::DmiSystemSlot::TYPE_PC98_C24:
            return "PC-98/C24";

        case proto::DmiSystemSlot::TYPE_PC98_E:
            return "PC-98/E";

        case proto::DmiSystemSlot::TYPE_PC98_LOCAL_BUS:
            return "PC-98/Local Bus";

        case proto::DmiSystemSlot::TYPE_PC98_CARD:
            return "PC-98/Card";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS:
            return "PCI Express";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X1:
            return "PCI Express x1";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X2:
            return "PCI Express x2";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X4:
            return "PCI Express x4";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X8:
            return "PCI Express x8";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X16:
            return "PCI Express x16";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2:
            return "PCI Express 2";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X1:
            return "PCI Express 2 x1";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X2:
            return "PCI Express 2 x2";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X4:
            return "PCI Express 2 x4";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X8:
            return "PCI Express 2 x8";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X16:
            return "PCI Express 2 x16";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3:
            return "PCI Express 3";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X1:
            return "PCI Express 3 x1";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X2:
            return "PCI Express 3 x2";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X4:
            return "PCI Express 3 x4";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X8:
            return "PCI Express 3 x8";

        case proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X16:
            return "PCI Express 3 x16";

        default:
            return "Unknown";
    }
}

const char* UsageToString(proto::DmiSystemSlot::Usage value)
{
    switch (value)
    {
        case proto::DmiSystemSlot::USAGE_OTHER:
            return "Other";

        case proto::DmiSystemSlot::USAGE_AVAILABLE:
            return "Available";

        case proto::DmiSystemSlot::USAGE_IN_USE:
            return "In Use";

        default:
            return "Unknown";
    }
}

const char* BusWidthToString(proto::DmiSystemSlot::BusWidth value)
{
    switch (value)
    {
        case proto::DmiSystemSlot::BUS_WIDTH_OTHER:
            return "Other";

        case proto::DmiSystemSlot::BUS_WIDTH_8_BIT:
            return "8-bit";

        case proto::DmiSystemSlot::BUS_WIDTH_16_BIT:
            return "16-bit";

        case proto::DmiSystemSlot::BUS_WIDTH_32_BIT:
            return "32-bit";

        case proto::DmiSystemSlot::BUS_WIDTH_64_BIT:
            return "64-bit";

        case proto::DmiSystemSlot::BUS_WIDTH_128_BIT:
            return "128-bit";

        case proto::DmiSystemSlot::BUS_WIDTH_X1:
            return "x1";

        case proto::DmiSystemSlot::BUS_WIDTH_X2:
            return "x2";

        case proto::DmiSystemSlot::BUS_WIDTH_X4:
            return "x4";

        case proto::DmiSystemSlot::BUS_WIDTH_X8:
            return "x8";

        case proto::DmiSystemSlot::BUS_WIDTH_X12:
            return "x12";

        case proto::DmiSystemSlot::BUS_WIDTH_X16:
            return "x16";

        case proto::DmiSystemSlot::BUS_WIDTH_X32:
            return "x32";

        default:
            return "Unknown";
    }
}

const char* LengthToString(proto::DmiSystemSlot::Length value)
{
    switch (value)
    {
        case proto::DmiSystemSlot::LENGTH_OTHER:
            return "Other";

        case proto::DmiSystemSlot::LENGTH_SHORT:
            return "Short";

        case proto::DmiSystemSlot::LENGTH_LONG:
            return "Long";

        default:
            return "Unknown";
    }
}

proto::DmiSystemSlot::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiSystemSlot::TYPE_OTHER;
        case 0x02: return proto::DmiSystemSlot::TYPE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlot::TYPE_ISA;
        case 0x04: return proto::DmiSystemSlot::TYPE_MCA;
        case 0x05: return proto::DmiSystemSlot::TYPE_EISA;
        case 0x06: return proto::DmiSystemSlot::TYPE_PCI;
        case 0x07: return proto::DmiSystemSlot::TYPE_PC_CARD;
        case 0x08: return proto::DmiSystemSlot::TYPE_VLB;
        case 0x09: return proto::DmiSystemSlot::TYPE_PROPRIETARY;
        case 0x0A: return proto::DmiSystemSlot::TYPE_PROCESSOR_CARD;
        case 0x0B: return proto::DmiSystemSlot::TYPE_PROPRIETARY_MEMORY_CARD;
        case 0x0C: return proto::DmiSystemSlot::TYPE_IO_RISER_CARD;
        case 0x0D: return proto::DmiSystemSlot::TYPE_NUBUS;
        case 0x0E: return proto::DmiSystemSlot::TYPE_PCI_66;
        case 0x0F: return proto::DmiSystemSlot::TYPE_AGP;
        case 0x10: return proto::DmiSystemSlot::TYPE_AGP_2X;
        case 0x11: return proto::DmiSystemSlot::TYPE_AGP_4X;
        case 0x12: return proto::DmiSystemSlot::TYPE_PCI_X;
        case 0x13: return proto::DmiSystemSlot::TYPE_AGP_8X;
        case 0x14: return proto::DmiSystemSlot::TYPE_M2_SOCKET_1DP;
        case 0x15: return proto::DmiSystemSlot::TYPE_M2_SOCKET_1SD;
        case 0x16: return proto::DmiSystemSlot::TYPE_M2_SOCKET_2;
        case 0x17: return proto::DmiSystemSlot::TYPE_M2_SOCKET_3;
        case 0x18: return proto::DmiSystemSlot::TYPE_MXM_TYPE_I;
        case 0x19: return proto::DmiSystemSlot::TYPE_MXM_TYPE_II;
        case 0x1A: return proto::DmiSystemSlot::TYPE_MXM_TYPE_III;
        case 0x1B: return proto::DmiSystemSlot::TYPE_MXM_TYPE_III_HE;
        case 0x1C: return proto::DmiSystemSlot::TYPE_MXM_TYPE_IV;
        case 0x1D: return proto::DmiSystemSlot::TYPE_MXM_30_TYPE_A;
        case 0x1E: return proto::DmiSystemSlot::TYPE_MXM_30_TYPE_B;
        case 0x1F: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_SFF_8639;
        case 0x20: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_SFF_8639;
        case 0x21: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_52PIN_WITH_BOTTOM_SIDE;
        case 0x22: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_52PIN;
        case 0x23: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_MINI_76PIN;

        case 0xA0: return proto::DmiSystemSlot::TYPE_PC98_C20;
        case 0xA1: return proto::DmiSystemSlot::TYPE_PC98_C24;
        case 0xA2: return proto::DmiSystemSlot::TYPE_PC98_E;
        case 0xA3: return proto::DmiSystemSlot::TYPE_PC98_LOCAL_BUS;
        case 0xA4: return proto::DmiSystemSlot::TYPE_PC98_CARD;
        case 0xA5: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS;
        case 0xA6: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X1;
        case 0xA7: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X2;
        case 0xA8: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X4;
        case 0xA9: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X8;
        case 0xAA: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_X16;
        case 0xAB: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2;
        case 0xAC: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X1;
        case 0xAD: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X2;
        case 0xAE: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X4;
        case 0xAF: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X8;
        case 0xB0: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_2_X16;
        case 0xB1: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3;
        case 0xB2: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X1;
        case 0xB3: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X2;
        case 0xB4: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X4;
        case 0xB5: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X8;
        case 0xB6: return proto::DmiSystemSlot::TYPE_PCI_EXPRESS_3_X16;

        default: return proto::DmiSystemSlot::TYPE_UNKNOWN;
    }
}

proto::DmiSystemSlot::Usage GetUsage(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiSystemSlot::USAGE_OTHER;
        case 0x02: return proto::DmiSystemSlot::USAGE_UNKNOWN;
        case 0x03: return proto::DmiSystemSlot::USAGE_AVAILABLE;
        case 0x04: return proto::DmiSystemSlot::USAGE_IN_USE;
        default: return proto::DmiSystemSlot::USAGE_UNKNOWN;
    }
}

proto::DmiSystemSlot::BusWidth GetBusWidth(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiSystemSlot::BUS_WIDTH_OTHER;
        case 0x02: return proto::DmiSystemSlot::BUS_WIDTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlot::BUS_WIDTH_8_BIT;
        case 0x04: return proto::DmiSystemSlot::BUS_WIDTH_16_BIT;
        case 0x05: return proto::DmiSystemSlot::BUS_WIDTH_32_BIT;
        case 0x06: return proto::DmiSystemSlot::BUS_WIDTH_64_BIT;
        case 0x07: return proto::DmiSystemSlot::BUS_WIDTH_128_BIT;
        case 0x08: return proto::DmiSystemSlot::BUS_WIDTH_X1;
        case 0x09: return proto::DmiSystemSlot::BUS_WIDTH_X2;
        case 0x0A: return proto::DmiSystemSlot::BUS_WIDTH_X4;
        case 0x0B: return proto::DmiSystemSlot::BUS_WIDTH_X8;
        case 0x0C: return proto::DmiSystemSlot::BUS_WIDTH_X12;
        case 0x0D: return proto::DmiSystemSlot::BUS_WIDTH_X16;
        case 0x0E: return proto::DmiSystemSlot::BUS_WIDTH_X32;
        default: return proto::DmiSystemSlot::BUS_WIDTH_OTHER;
    }
}

proto::DmiSystemSlot::Length GetLength(uint8_t value)
{
    switch (value)
    {
        case 0x01: return proto::DmiSystemSlot::LENGTH_OTHER;
        case 0x02: return proto::DmiSystemSlot::LENGTH_UNKNOWN;
        case 0x03: return proto::DmiSystemSlot::LENGTH_SHORT;
        case 0x04: return proto::DmiSystemSlot::LENGTH_LONG;
        default: return proto::DmiSystemSlot::LENGTH_UNKNOWN;
    }
}

} // namespace

CategoryDmiSystemSlot::CategoryDmiSystemSlot()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiSystemSlot::Name() const
{
    return "System Slots";
}

Category::IconId CategoryDmiSystemSlot::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiSystemSlot::Guid() const
{
    return "{7A4F71C6-557F-48A5-AC94-E430F69154F1}";
}

void CategoryDmiSystemSlot::Parse(Table& table, const std::string& data)
{
    proto::DmiSystemSlot message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiSystemSlot::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("System Slot #%d", index + 1));

        if (!item.slot_designation().empty())
            group.AddParam("Slot Designation", Value::String(item.slot_designation()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));
        group.AddParam("Usage", Value::String(UsageToString(item.usage())));
        group.AddParam("Bus Width", Value::String(BusWidthToString(item.bus_width())));
        group.AddParam("Length", Value::String(LengthToString(item.length())));
    }
}

std::string CategoryDmiSystemSlot::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiSystemSlot message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_SYSTEM_SLOT);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();
        if (table.GetTableLength() < 0x0C)
            continue;

        proto::DmiSystemSlot::Item* item = message.add_item();

        item->set_slot_designation(table.GetString(0x04));
        item->set_type(GetType(table.GetByte(0x05)));
        item->set_usage(GetUsage(table.GetByte(0x07)));
        item->set_bus_width(GetBusWidth(table.GetByte(0x06)));
        item->set_length(GetLength(table.GetByte(0x08)));
    }

    return message.SerializeAsString();
}

} // namespace aspia
