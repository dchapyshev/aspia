//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_system.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "system_info/category_dmi_system.h"
#include "system_info/category_dmi_system.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* WakeupTypeToString(proto::DmiSystem::WakeupType value)
{
    switch (value)
    {
        case proto::DmiSystem::WAKEUP_TYPE_OTHER:
            return "Other";

        case proto::DmiSystem::WAKEUP_TYPE_APM_TIMER:
            return "APM Timer";

        case proto::DmiSystem::WAKEUP_TYPE_MODEM_RING:
            return "Modem Ring";

        case proto::DmiSystem::WAKEUP_TYPE_LAN_REMOTE:
            return "LAN Remote";

        case proto::DmiSystem::WAKEUP_TYPE_POWER_SWITCH:
            return "Power Switch";

        case proto::DmiSystem::WAKEUP_TYPE_PCI_PME:
            return "PCI PME#";

        case proto::DmiSystem::WAKEUP_TYPE_AC_POWER_RESTORED:
            return "AC Power Restored";

        default:
            return "Unknown";
    }
}

std::string GetUUID(const SMBios::TableReader& table)
{
    if (table.GetTableLength() < 0x19)
        return std::string();

    const uint8_t* ptr = table.GetPointer(0x08);

    bool only_0xFF = true;
    bool only_0x00 = true;

    for (int i = 0; i < 16 && (only_0x00 || only_0xFF); ++i)
    {
        if (ptr[i] != 0x00) only_0x00 = false;
        if (ptr[i] != 0xFF) only_0xFF = false;
    }

    if (only_0xFF || only_0x00)
        return std::string();

    if ((table.GetMajorVersion() << 8) + table.GetMinorVersion() >= 0x0206)
    {
        return StringPrintf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                            ptr[3], ptr[2], ptr[1], ptr[0], ptr[5], ptr[4], ptr[7], ptr[6],
                            ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
    }

    return StringPrintf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                        ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                        ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
}

proto::DmiSystem::WakeupType GetWakeupType(const SMBios::TableReader& table)
{
    if (table.GetTableLength() < 0x19)
        return proto::DmiSystem::WAKEUP_TYPE_UNKNOWN;

    switch (table.GetByte(0x18))
    {
        case 0x01: return proto::DmiSystem::WAKEUP_TYPE_OTHER;
        case 0x02: return proto::DmiSystem::WAKEUP_TYPE_UNKNOWN;
        case 0x03: return proto::DmiSystem::WAKEUP_TYPE_APM_TIMER;
        case 0x04: return proto::DmiSystem::WAKEUP_TYPE_MODEM_RING;
        case 0x05: return proto::DmiSystem::WAKEUP_TYPE_LAN_REMOTE;
        case 0x06: return proto::DmiSystem::WAKEUP_TYPE_POWER_SWITCH;
        case 0x07: return proto::DmiSystem::WAKEUP_TYPE_PCI_PME;
        case 0x08: return proto::DmiSystem::WAKEUP_TYPE_AC_POWER_RESTORED;
        default: return proto::DmiSystem::WAKEUP_TYPE_UNKNOWN;
    }
}

std::string GetSKUNumber(const SMBios::TableReader& table)
{
    if (table.GetTableLength() < 0x1B)
        return std::string();

    return table.GetString(0x19);
}

std::string GetFamily(const SMBios::TableReader& table)
{
    if (table.GetTableLength() < 0x1B)
        return std::string();

    return table.GetString(0x1A);
}

} // namespace

CategoryDmiSystem::CategoryDmiSystem()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

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
    return "{F599BBA4-AEBB-4583-A15E-9848F4C98601}";
}

void CategoryDmiSystem::Parse(Table& table, const std::string& data)
{
    proto::DmiSystem message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    if (!message.manufacturer().empty())
        table.AddParam("Manufacturer", Value::String(message.manufacturer()));

    if (!message.product_name().empty())
        table.AddParam("Product Name", Value::String(message.product_name()));

    if (!message.version().empty())
        table.AddParam("Version", Value::String(message.version()));

    if (!message.serial_number().empty())
        table.AddParam("Serial Number", Value::String(message.serial_number()));

    if (!message.uuid().empty())
        table.AddParam("UUID", Value::String(message.uuid()));

    table.AddParam("Wakeup Type", Value::String(WakeupTypeToString(message.wakeup_type())));

    if (!message.sku_number().empty())
        table.AddParam("SKU Number", Value::String(message.sku_number()));

    if (!message.family().empty())
        table.AddParam("Family", Value::String(message.family()));
}

std::string CategoryDmiSystem::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumeratorNew table_enumerator(*smbios, SMBios::TABLE_TYPE_SYSTEM);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::TableReader table = table_enumerator.GetTable();
    proto::DmiSystem message;

    message.set_manufacturer(table.GetString(0x04));
    message.set_product_name(table.GetString(0x05));
    message.set_version(table.GetString(0x06));
    message.set_serial_number(table.GetString(0x07));
    message.set_uuid(GetUUID(table));
    message.set_wakeup_type(GetWakeupType(table));
    message.set_sku_number(GetSKUNumber(table));
    message.set_family(GetFamily(table));

    return message.SerializeAsString();
}

} // namespace aspia
