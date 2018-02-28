//
// PROJECT:         Aspia
// FILE:            category/category_dmi_bios.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "base/strings/string_printf.h"
#include "base/bitset.h"
#include "category/category_dmi_bios.h"
#include "category/category_dmi_bios.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

uint64_t GetSize(const SMBios::Table& table)
{
    const uint8_t old_size = table.Byte(0x09);
    if (old_size != 0xFF)
        return (old_size + 1) << 6;

    BitSet<uint16_t> bitfield(table.Word(0x18));

    uint16_t size = 16; // By default 16 MBytes.

    if (table.Length() >= 0x1A)
        size = bitfield.Range(0, 13);

    switch (bitfield.Range(14, 15))
    {
        case 0x0000: // MB
            return static_cast<uint64_t>(size) * 1024ULL;

        case 0x0001: // GB
            return static_cast<uint64_t>(size) * 1024ULL * 1024ULL;

        default:
            return 0;
    }
}

std::string GetBiosRevision(const SMBios::Table& table)
{
    const uint8_t major = table.Byte(0x14);
    const uint8_t minor = table.Byte(0x15);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string GetFirmwareRevision(const SMBios::Table& table)
{
    const uint8_t major = table.Byte(0x16);
    const uint8_t minor = table.Byte(0x17);

    if (major != 0xFF && minor != 0xFF)
        return StringPrintf("%u.%u", major, minor);

    return std::string();
}

std::string GetAddress(const SMBios::Table& table)
{
    const uint16_t address = table.Word(0x06);

    if (address != 0)
        return StringPrintf("%04X0h", address);

    return std::string();
}

int GetRuntimeSize(const SMBios::Table& table)
{
    const uint16_t address = table.Word(0x06);
    if (address == 0)
        return 0;

    const uint32_t code = (0x10000 - address) << 4;

    if (code & 0x000003FF)
        return code;

    return (code >> 10) * 1024;
}

} // namespace

CategoryDmiBios::CategoryDmiBios()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

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
    return "{B0B73D57-2CDC-4814-9AE0-C7AF7DDDD60E}";
}

void CategoryDmiBios::Parse(Table& table, const std::string& data)
{
    proto::DmiBios message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    if (!message.manufacturer().empty())
        table.AddParam("Manufacturer", Value::String(message.manufacturer()));

    if (!message.version().empty())
        table.AddParam("Version", Value::String(message.version()));

    if (!message.date().empty())
        table.AddParam("Date", Value::String(message.date()));

    if (message.size() != 0)
        table.AddParam("Size", Value::Number(message.size(), "kB"));

    if (!message.bios_revision().empty())
        table.AddParam("BIOS Revision", Value::String(message.bios_revision()));

    if (!message.firmware_revision().empty())
        table.AddParam("Firmware Revision", Value::String(message.firmware_revision()));

    if (!message.address().empty())
        table.AddParam("Address", Value::String(message.address()));

    if (message.runtime_size() != 0)
        table.AddParam("Runtime Size", Value::Number(message.runtime_size(), "Bytes"));

    if (message.has_characteristics())
    {
        std::vector<std::pair<std::string, bool>> list;

        auto add = [&](const char* name, bool is_supported)
        {
            list.emplace_back(name, is_supported);
        };

        const proto::DmiBios::Characteristics& ch = message.characteristics();

        add("ISA", ch.has_isa());
        add("MCA", ch.has_mca());
        add("EISA", ch.has_eisa());
        add("PCI", ch.has_pci());
        add("PC Card (PCMCIA)", ch.has_pc_card());
        add("Plug and Play", ch.has_pnp());
        add("APM is Supported", ch.has_apm());
        add("BIOS is Upgradeable (Flash)", ch.has_bios_upgradeable());
        add("BIOS Shadowing is Allowed", ch.has_bios_shadowing());
        add("VL-VESA", ch.has_vlb());
        add("ESCD Support is Available", ch.has_escd());
        add("Boot from CD", ch.has_boot_from_cd());
        add("Selectable Boot", ch.has_selectable_boot());
        add("BIOS ROM is Socketed", ch.has_socketed_boot_rom());
        add("Boot from PC card (PCMCIA)", ch.has_boot_from_pc_card());
        add("EDD Specification is Supported", ch.has_edd());
        add("Japanese Floppy for NEC 9800 1.2 MB (int 13h)", ch.has_japanese_floppy_for_nec9800());
        add("Japanese Floppy for Toshiba 1.2 MB (int 13h)", ch.has_japanece_floppy_for_toshiba());
        add("5.25\"/360 kB Floppy (int 13h)", ch.has_525_360kb_floppy());
        add("5.25\"/1.2 MB Floppy (int 13h)", ch.has_525_12mb_floppy());
        add("3.5\"/720 kB Floppy (int 13h)", ch.has_35_720kb_floppy());
        add("3.5\"/2.88 MB Floppy (int 13h)", ch.has_35_288mb_floppy());
        add("Print Screen (int 5h)", ch.has_print_screen());
        add("8042 Keyboard (int 9h)", ch.has_8042_keyboard());
        add("Serial (int 14h)", ch.has_serial());
        add("Printer (int 17h)", ch.has_printer());
        add("CGA/Mono Video (int 10h)", ch.has_cga_video());
        add("NEC PC-98", ch.has_nec_pc98());

        add("ACPI", ch.has_acpi());
        add("USB Legacy", ch.has_usb_legacy());
        add("AGP", ch.has_agp());
        add("I2O Boot", ch.has_i2o_boot());
        add("LS-120 SuperDisk Boot", ch.has_ls120_boot());
        add("ATAPI ZIP Drive Boot", ch.has_atapi_zip_drive_boot());
        add("IEEE 1394 Boot", ch.has_ieee1394_boot());
        add("Smart Battery", ch.has_smart_battery());

        add("BIOS Boot Specification", ch.has_bios_boot_specification());
        add("Function Key-initiated Network Boot", ch.has_key_init_network_boot());
        add("Targeted Content Distribution", ch.has_targeted_content_distrib());
        add("UEFI Specification", ch.has_uefi());
        add("Virtual Machine", ch.has_virtual_machine());

        std::sort(list.begin(), list.end());

        Group group = table.AddGroup("Characteristics");

        for (const auto& list_item : list)
        {
            group.AddParam(list_item.first, Value::Bool(list_item.second));
        }
    }
}

std::string CategoryDmiBios::Serialize()
{
    std::unique_ptr<SMBios> smbios = ReadSMBios();
    if (!smbios)
        return std::string();

    SMBios::TableEnumerator table_enumerator(*smbios, SMBios::TABLE_TYPE_BIOS);
    if (table_enumerator.IsAtEnd())
        return std::string();

    SMBios::Table table = table_enumerator.GetTable();
    proto::DmiBios message;

    message.set_manufacturer(table.String(0x04));
    message.set_version(table.String(0x05));
    message.set_date(table.String(0x08));
    message.set_size(GetSize(table));
    message.set_bios_revision(GetBiosRevision(table));
    message.set_firmware_revision(GetFirmwareRevision(table));
    message.set_address(GetAddress(table));
    message.set_runtime_size(GetRuntimeSize(table));

    proto::DmiBios::Characteristics* item = message.mutable_characteristics();

    BitSet<uint64_t> characteristics = table.Qword(0x0A);
    if (!characteristics.Test(3))
    {
        item->set_has_isa(characteristics.Test(4));
        item->set_has_mca(characteristics.Test(5));
        item->set_has_eisa(characteristics.Test(6));
        item->set_has_pci(characteristics.Test(7));
        item->set_has_pc_card(characteristics.Test(8));
        item->set_has_pnp(characteristics.Test(9));
        item->set_has_apm(characteristics.Test(10));
        item->set_has_bios_upgradeable(characteristics.Test(11));
        item->set_has_bios_shadowing(characteristics.Test(12));
        item->set_has_vlb(characteristics.Test(13));
        item->set_has_escd(characteristics.Test(14));
        item->set_has_boot_from_cd(characteristics.Test(15));
        item->set_has_selectable_boot(characteristics.Test(16));
        item->set_has_socketed_boot_rom(characteristics.Test(17));
        item->set_has_boot_from_pc_card(characteristics.Test(18));
        item->set_has_edd(characteristics.Test(19));
        item->set_has_japanese_floppy_for_nec9800(characteristics.Test(20));
        item->set_has_japanece_floppy_for_toshiba(characteristics.Test(21));
        item->set_has_525_360kb_floppy(characteristics.Test(22));
        item->set_has_525_12mb_floppy(characteristics.Test(23));
        item->set_has_35_720kb_floppy(characteristics.Test(24));
        item->set_has_35_288mb_floppy(characteristics.Test(25));
        item->set_has_print_screen(characteristics.Test(26));
        item->set_has_8042_keyboard(characteristics.Test(27));
        item->set_has_serial(characteristics.Test(28));
        item->set_has_printer(characteristics.Test(29));
        item->set_has_cga_video(characteristics.Test(30));
        item->set_has_nec_pc98(characteristics.Test(31));
    }

    if (table.Length() >= 0x13)
    {
        BitSet<uint8_t> characteristics1 = table.Byte(0x12);

        item->set_has_acpi(characteristics1.Test(0));
        item->set_has_usb_legacy(characteristics1.Test(1));
        item->set_has_agp(characteristics1.Test(2));
        item->set_has_i2o_boot(characteristics1.Test(3));
        item->set_has_ls120_boot(characteristics1.Test(4));
        item->set_has_atapi_zip_drive_boot(characteristics1.Test(5));
        item->set_has_ieee1394_boot(characteristics1.Test(6));
        item->set_has_smart_battery(characteristics1.Test(7));
    }

    if (table.Length() >= 0x14)
    {
        BitSet<uint8_t> characteristics2 = table.Byte(0x13);

        item->set_has_bios_boot_specification(characteristics2.Test(0));
        item->set_has_key_init_network_boot(characteristics2.Test(1));
        item->set_has_targeted_content_distrib(characteristics2.Test(2));
        item->set_has_uefi(characteristics2.Test(3));
        item->set_has_virtual_machine(characteristics2.Test(4));
    }

    return message.SerializeAsString();
}

} // namespace aspia
