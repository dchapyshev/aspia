//
// PROJECT:         Aspia
// FILE:            category/category_dmi_port_connector.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "category/category_dmi_port_connector.h"
#include "category/category_dmi_port_connector.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* TypeToString(proto::DmiPortConnector::Type value)
{
    switch (value)
    {
        case proto::DmiPortConnector::TYPE_NONE:
            return "None";

        case proto::DmiPortConnector::TYPE_PARALLEL_PORT_XT_AT_COMPATIBLE:
            return "Parallel Port XT/AT Compatible";

        case proto::DmiPortConnector::TYPE_PARALLEL_PORT_PS_2:
            return "Parallel Port PS/2";

        case proto::DmiPortConnector::TYPE_PARALLEL_PORT_ECP:
            return "Parallel Port ECP";

        case proto::DmiPortConnector::TYPE_PARALLEL_PORT_EPP:
            return "Parallel Port EPP";

        case proto::DmiPortConnector::TYPE_PARALLEL_PORT_ECP_EPP:
            return "Parallel Port ECP/EPP";

        case proto::DmiPortConnector::TYPE_SERIAL_PORT_XT_AT_COMPATIBLE:
            return "Serial Port XT/AT Compatible";

        case proto::DmiPortConnector::TYPE_SERIAL_PORT_16450_COMPATIBLE:
            return "Serial Port 16450 Compatible";

        case proto::DmiPortConnector::TYPE_SERIAL_PORT_16550_COMPATIBLE:
            return "Serial Port 16550 Compatible";

        case proto::DmiPortConnector::TYPE_SERIAL_PORT_16550A_COMPATIBLE:
            return "Serial Port 16550A Compatible";

        case proto::DmiPortConnector::TYPE_SCSI_PORT:
            return "SCSI Port";

        case proto::DmiPortConnector::TYPE_MIDI_PORT:
            return "MIDI Port";

        case proto::DmiPortConnector::TYPE_JOYSTICK_PORT:
            return "Joystick Port";

        case proto::DmiPortConnector::TYPE_KEYBOARD_PORT:
            return "Keyboard Port";

        case proto::DmiPortConnector::TYPE_MOUSE_PORT:
            return "Mouse Port";

        case proto::DmiPortConnector::TYPE_SSA_SCSI:
            return "SSA SCSI";

        case proto::DmiPortConnector::TYPE_USB:
            return "USB";

        case proto::DmiPortConnector::TYPE_FIREWIRE:
            return "Firewire (IEEE 1394)";

        case proto::DmiPortConnector::TYPE_PCMCIA_TYPE_I:
            return "PCMCIA Type I";

        case proto::DmiPortConnector::TYPE_PCMCIA_TYPE_II:
            return "PCMCIA Type II";

        case proto::DmiPortConnector::TYPE_PCMCIA_TYPE_III:
            return "PCMCIA Type III";

        case proto::DmiPortConnector::TYPE_CARDBUS:
            return "Cardbus";

        case proto::DmiPortConnector::TYPE_ACCESS_BUS_PORT:
            return "Access Bus Port";

        case proto::DmiPortConnector::TYPE_SCSI_II:
            return "SCSI II";

        case proto::DmiPortConnector::TYPE_SCSI_WIDE:
            return "SCSI Wide";

        case proto::DmiPortConnector::TYPE_PC_98:
            return "PC-98";

        case proto::DmiPortConnector::TYPE_PC_98_HIRESO:
            return "PC-98 Hireso";

        case proto::DmiPortConnector::TYPE_PC_H98:
            return "PC-H98";

        case proto::DmiPortConnector::TYPE_VIDEO_PORT:
            return "Video Port";

        case proto::DmiPortConnector::TYPE_AUDIO_PORT:
            return "Audio Port";

        case proto::DmiPortConnector::TYPE_MODEM_PORT:
            return "Modem Port";

        case proto::DmiPortConnector::TYPE_NETWORK_PORT:
            return "Network Port";

        case proto::DmiPortConnector::TYPE_SATA:
            return "SATA";

        case proto::DmiPortConnector::TYPE_SAS:
            return "SAS";

        case proto::DmiPortConnector::TYPE_8251_COMPATIBLE:
            return "8251 Compatible";

        case proto::DmiPortConnector::TYPE_8251_FIFO_COMPATIBLE:
            return "8251 FIFO Compatible";

        default:
            return "Unknown";
    }
}

const char* ConnectorTypeToString(proto::DmiPortConnector::ConnectorType value)
{
    switch (value)
    {
        case proto::DmiPortConnector::CONNECTOR_TYPE_NONE:
            return "None";

        case proto::DmiPortConnector::CONNECTOR_TYPE_OTHER:
            return "Other";

        case proto::DmiPortConnector::CONNECTOR_TYPE_CENTRONICS:
            return "Centronics";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS:
            return "Mini Centronics";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PROPRIETARY:
            return "Proprietary";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_25_MALE:
            return "DB-25 male";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_25_FEMALE:
            return "DB-25 female";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_15_MALE:
            return "DB-15 male";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_15_FEMALE:
            return "DB-15 female";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_9_MALE:
            return "DB-9 male";

        case proto::DmiPortConnector::CONNECTOR_TYPE_DB_9_FEMALE:
            return "DB-9 female";

        case proto::DmiPortConnector::CONNECTOR_TYPE_RJ_11:
            return "RJ-11";

        case proto::DmiPortConnector::CONNECTOR_TYPE_RJ_45:
            return "RJ-45";

        case proto::DmiPortConnector::CONNECTOR_TYPE_50_PIN_MINISCSI:
            return "50 Pin MiniSCSI";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MINI_DIN:
            return "Mini DIN";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MICRO_DIN:
            return "Micro DIN";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PS_2:
            return "PS/2";

        case proto::DmiPortConnector::CONNECTOR_TYPE_INFRARED:
            return "Infrared";

        case proto::DmiPortConnector::CONNECTOR_TYPE_HP_HIL:
            return "HP-HIL";

        case proto::DmiPortConnector::CONNECTOR_TYPE_ACCESS_BUS_USB:
            return "Access Bus (USB)";

        case proto::DmiPortConnector::CONNECTOR_TYPE_SSA_SCSI:
            return "SSA SCSI";

        case proto::DmiPortConnector::CONNECTOR_TYPE_CIRCULAR_DIN_8_MALE:
            return "Circular DIN-8 male";

        case proto::DmiPortConnector::CONNECTOR_TYPE_CIRCULAR_DIN_8_FEMALE:
            return "Circular DIN-8 female";

        case proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_IDE:
            return "On Board IDE";

        case proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_FLOPPY:
            return "On Board Floppy";

        case proto::DmiPortConnector::CONNECTOR_TYPE_9_PIN_DUAL_INLINE:
            return "9 Pin Dual Inline (pin 10 cut)";

        case proto::DmiPortConnector::CONNECTOR_TYPE_25_PIN_DUAL_INLINE:
            return "25 Pin Dual Inline (pin 26 cut)";

        case proto::DmiPortConnector::CONNECTOR_TYPE_50_PIN_DUAL_INLINE:
            return "50 Pin Dual Inline";

        case proto::DmiPortConnector::CONNECTOR_TYPE_68_PIN_DUAL_INLINE:
            return "68 Pin Dual Inline";

        case proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_SOUND_INPUT_FROM_CDROM:
            return "On Board Sound Input From CD-ROM";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_14:
            return "Mini Centronics Type-14";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_26:
            return "Mini Centronics Type-26";

        case proto::DmiPortConnector::CONNECTOR_TYPE_MINI_JACK:
            return "Mini Jack (headphones)";

        case proto::DmiPortConnector::CONNECTOR_TYPE_BNC:
            return "BNC";

        case proto::DmiPortConnector::CONNECTOR_TYPE_IEEE_1394:
            return "IEEE 1394";

        case proto::DmiPortConnector::CONNECTOR_TYPE_SAS_SATE_PLUG_RECEPTACLE:
            return "SAS/SATA Plug Receptacle";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PC_98:
            return "PC-98";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_HIRESO:
            return "PC-98 Hireso";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PC_H98:
            return "PC-H98";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_NOTE:
            return "PC-98 Note";

        case proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_FULL:
            return "PC-98 Full";

        default:
            return "Unknown";
    }
}

proto::DmiPortConnector::Type GetType(uint8_t value)
{
    switch (value)
    {
        case 0x00: return proto::DmiPortConnector::TYPE_NONE;
        case 0x01: return proto::DmiPortConnector::TYPE_PARALLEL_PORT_XT_AT_COMPATIBLE;
        case 0x02: return proto::DmiPortConnector::TYPE_PARALLEL_PORT_PS_2;
        case 0x03: return proto::DmiPortConnector::TYPE_PARALLEL_PORT_ECP;
        case 0x04: return proto::DmiPortConnector::TYPE_PARALLEL_PORT_EPP;
        case 0x05: return proto::DmiPortConnector::TYPE_PARALLEL_PORT_ECP_EPP;
        case 0x06: return proto::DmiPortConnector::TYPE_SERIAL_PORT_XT_AT_COMPATIBLE;
        case 0x07: return proto::DmiPortConnector::TYPE_SERIAL_PORT_16450_COMPATIBLE;
        case 0x08: return proto::DmiPortConnector::TYPE_SERIAL_PORT_16550_COMPATIBLE;
        case 0x09: return proto::DmiPortConnector::TYPE_SERIAL_PORT_16550A_COMPATIBLE;
        case 0x0A: return proto::DmiPortConnector::TYPE_SCSI_PORT;
        case 0x0B: return proto::DmiPortConnector::TYPE_MIDI_PORT;
        case 0x0C: return proto::DmiPortConnector::TYPE_JOYSTICK_PORT;
        case 0x0D: return proto::DmiPortConnector::TYPE_KEYBOARD_PORT;
        case 0x0E: return proto::DmiPortConnector::TYPE_MOUSE_PORT;
        case 0x0F: return proto::DmiPortConnector::TYPE_SSA_SCSI;
        case 0x10: return proto::DmiPortConnector::TYPE_USB;
        case 0x11: return proto::DmiPortConnector::TYPE_FIREWIRE;
        case 0x12: return proto::DmiPortConnector::TYPE_PCMCIA_TYPE_I;
        case 0x13: return proto::DmiPortConnector::TYPE_PCMCIA_TYPE_II;
        case 0x14: return proto::DmiPortConnector::TYPE_PCMCIA_TYPE_III;
        case 0x15: return proto::DmiPortConnector::TYPE_CARDBUS;
        case 0x16: return proto::DmiPortConnector::TYPE_ACCESS_BUS_PORT;
        case 0x17: return proto::DmiPortConnector::TYPE_SCSI_II;
        case 0x18: return proto::DmiPortConnector::TYPE_SCSI_WIDE;
        case 0x19: return proto::DmiPortConnector::TYPE_PC_98;
        case 0x1A: return proto::DmiPortConnector::TYPE_PC_98_HIRESO;
        case 0x1B: return proto::DmiPortConnector::TYPE_PC_H98;
        case 0x1C: return proto::DmiPortConnector::TYPE_VIDEO_PORT;
        case 0x1D: return proto::DmiPortConnector::TYPE_AUDIO_PORT;
        case 0x1E: return proto::DmiPortConnector::TYPE_MODEM_PORT;
        case 0x1F: return proto::DmiPortConnector::TYPE_NETWORK_PORT;
        case 0x20: return proto::DmiPortConnector::TYPE_SATA;
        case 0x21: return proto::DmiPortConnector::TYPE_SAS;

        case 0xA0: return proto::DmiPortConnector::TYPE_8251_COMPATIBLE;
        case 0xA1: return proto::DmiPortConnector::TYPE_8251_FIFO_COMPATIBLE;

        default: return proto::DmiPortConnector::TYPE_UNKNOWN;
    }
}

proto::DmiPortConnector::ConnectorType ConnectorType(uint8_t value)
{
    switch (value)
    {
        case 0x00: return proto::DmiPortConnector::CONNECTOR_TYPE_NONE;
        case 0x01: return proto::DmiPortConnector::CONNECTOR_TYPE_CENTRONICS;
        case 0x02: return proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS;
        case 0x03: return proto::DmiPortConnector::CONNECTOR_TYPE_PROPRIETARY;
        case 0x04: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_25_MALE;
        case 0x05: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_25_FEMALE;
        case 0x06: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_15_MALE;
        case 0x07: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_15_FEMALE;
        case 0x08: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_9_MALE;
        case 0x09: return proto::DmiPortConnector::CONNECTOR_TYPE_DB_9_FEMALE;
        case 0x0A: return proto::DmiPortConnector::CONNECTOR_TYPE_RJ_11;
        case 0x0B: return proto::DmiPortConnector::CONNECTOR_TYPE_RJ_45;
        case 0x0C: return proto::DmiPortConnector::CONNECTOR_TYPE_50_PIN_MINISCSI;
        case 0x0D: return proto::DmiPortConnector::CONNECTOR_TYPE_MINI_DIN;
        case 0x0E: return proto::DmiPortConnector::CONNECTOR_TYPE_MICRO_DIN;
        case 0x0F: return proto::DmiPortConnector::CONNECTOR_TYPE_PS_2;
        case 0x10: return proto::DmiPortConnector::CONNECTOR_TYPE_INFRARED;
        case 0x11: return proto::DmiPortConnector::CONNECTOR_TYPE_HP_HIL;
        case 0x12: return proto::DmiPortConnector::CONNECTOR_TYPE_ACCESS_BUS_USB;
        case 0x13: return proto::DmiPortConnector::CONNECTOR_TYPE_SSA_SCSI;
        case 0x14: return proto::DmiPortConnector::CONNECTOR_TYPE_CIRCULAR_DIN_8_MALE;
        case 0x15: return proto::DmiPortConnector::CONNECTOR_TYPE_CIRCULAR_DIN_8_FEMALE;
        case 0x16: return proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_IDE;
        case 0x17: return proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_FLOPPY;
        case 0x18: return proto::DmiPortConnector::CONNECTOR_TYPE_9_PIN_DUAL_INLINE;
        case 0x19: return proto::DmiPortConnector::CONNECTOR_TYPE_25_PIN_DUAL_INLINE;
        case 0x1A: return proto::DmiPortConnector::CONNECTOR_TYPE_50_PIN_DUAL_INLINE;
        case 0x1B: return proto::DmiPortConnector::CONNECTOR_TYPE_68_PIN_DUAL_INLINE;
        case 0x1C: return proto::DmiPortConnector::CONNECTOR_TYPE_ONBOARD_SOUND_INPUT_FROM_CDROM;
        case 0x1D: return proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_14;
        case 0x1E: return proto::DmiPortConnector::CONNECTOR_TYPE_MINI_CENTRONICS_TYPE_26;
        case 0x1F: return proto::DmiPortConnector::CONNECTOR_TYPE_MINI_JACK;
        case 0x20: return proto::DmiPortConnector::CONNECTOR_TYPE_BNC;
        case 0x21: return proto::DmiPortConnector::CONNECTOR_TYPE_IEEE_1394;
        case 0x22: return proto::DmiPortConnector::CONNECTOR_TYPE_SAS_SATE_PLUG_RECEPTACLE;

        case 0xA0: return proto::DmiPortConnector::CONNECTOR_TYPE_PC_98;
        case 0xA1: return proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_HIRESO;
        case 0xA2: return proto::DmiPortConnector::CONNECTOR_TYPE_PC_H98;
        case 0xA3: return proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_NOTE;
        case 0xA4: return proto::DmiPortConnector::CONNECTOR_TYPE_PC_98_FULL;

        case 0xFF: return proto::DmiPortConnector::CONNECTOR_TYPE_OTHER;

        default: return proto::DmiPortConnector::CONNECTOR_TYPE_UNKNOWN;
    }
}

} // namespace

CategoryDmiPortConnector::CategoryDmiPortConnector()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryDmiPortConnector::Name() const
{
    return "Port Connectors";
}

Category::IconId CategoryDmiPortConnector::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiPortConnector::Guid() const
{
    return "{FF4CE0FE-261F-46EF-852F-42420E68CFD2}";
}

void CategoryDmiPortConnector::Parse(Table& table, const std::string& data)
{
    proto::DmiPortConnector message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::DmiPortConnector::Item& item = message.item(index);

        Group group = table.AddGroup(StringPrintf("Port Connector #%d", index + 1));

        if (!item.internal_designation().empty())
            group.AddParam("Internal Designation", Value::String(item.internal_designation()));

        if (!item.external_designation().empty())
            group.AddParam("External Designation", Value::String(item.external_designation()));

        group.AddParam("Type", Value::String(TypeToString(item.type())));

        group.AddParam("Internal Connector Type",
                       Value::String(ConnectorTypeToString(item.internal_connector_type())));
        group.AddParam("External Connector Type",
                       Value::String(ConnectorTypeToString(item.external_connector_type())));
    }
}

std::string CategoryDmiPortConnector::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    proto::DmiPortConnector message;

    for (SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_PORT_CONNECTOR);
         !table_enumerator.IsAtEnd();
         table_enumerator.Advance())
    {
        SMBios::Table table = table_enumerator.GetTable();
        if (table.Length() < 0x09)
            continue;

        proto::DmiPortConnector::Item* item = message.add_item();

        item->set_internal_designation(table.String(0x04));
        item->set_external_designation(table.String(0x06));
        item->set_type(GetType(table.Byte(0x08)));
        item->set_internal_connector_type(ConnectorType(table.Byte(0x05)));
        item->set_external_connector_type(ConnectorType(table.Byte(0x07)));
    }

    return message.SerializeAsString();
}

} // namespace aspia
