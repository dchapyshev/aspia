//
// PROJECT:         Aspia
// FILE:            system_info/category_dmi_bios.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/smbios_reader.h"
#include "system_info/category_dmi_bios.h"
#include "system_info/category_dmi_bios.pb.h"
#include "ui/resource.h"

namespace aspia {

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

    proto::DmiBios::Characteristics* characteristics = message.mutable_characteristics();

    characteristics->set_has_isa(table.HasISA());
    characteristics->set_has_mca(table.HasMCA());
    characteristics->set_has_eisa(table.HasEISA());
    characteristics->set_has_pci(table.HasPCI());
    characteristics->set_has_pc_card(table.HasPCCard());
    characteristics->set_has_pnp(table.HasPNP());
    characteristics->set_has_apm(table.HasAPM());
    characteristics->set_has_bios_upgradeable(table.HasBiosUpgradeable());
    characteristics->set_has_bios_shadowing(table.HasBiosShadowing());
    characteristics->set_has_vlb(table.HasVLB());
    characteristics->set_has_escd(table.HasESCD());
    characteristics->set_has_boot_from_cd(table.HasBootFromCD());
    characteristics->set_has_selectable_boot(table.HasSelectableBoot());
    characteristics->set_has_socketed_boot_rom(table.HasSocketedBootROM());
    characteristics->set_has_boot_from_pc_card(table.HasBootFromPCCard());
    characteristics->set_has_edd(table.HasEDD());
    characteristics->set_has_japanese_floppy_for_nec9800(table.HasJapaneseFloppyForNec9800());
    characteristics->set_has_japanece_floppy_for_toshiba(table.HasJapaneceFloppyForToshiba());
    characteristics->set_has_525_360kb_floppy(table.Has525_360kbFloppy());
    characteristics->set_has_525_12mb_floppy(table.Has525_12mbFloppy());
    characteristics->set_has_35_720kb_floppy(table.Has35_720kbFloppy());
    characteristics->set_has_35_288mb_floppy(table.Has35_288mbFloppy());
    characteristics->set_has_print_screen(table.HasPrintScreen());
    characteristics->set_has_8042_keyboard(table.Has8042Keyboard());
    characteristics->set_has_serial(table.HasSerial());
    characteristics->set_has_printer(table.HasPrinter());
    characteristics->set_has_cga_video(table.HasCGAVideo());
    characteristics->set_has_nec_pc98(table.HasNecPC98());
    characteristics->set_has_acpi(table.HasACPI());
    characteristics->set_has_usb_legacy(table.HasUSBLegacy());
    characteristics->set_has_agp(table.HasAGP());
    characteristics->set_has_i2o_boot(table.HasI2OBoot());
    characteristics->set_has_ls120_boot(table.HasLS120Boot());
    characteristics->set_has_atapi_zip_drive_boot(table.HasAtapiZipDriveBoot());
    characteristics->set_has_ieee1394_boot(table.HasIeee1394Boot());
    characteristics->set_has_smart_battery(table.HasSmartBattery());
    characteristics->set_has_bios_boot_specification(table.HasBiosBootSpecification());
    characteristics->set_has_key_init_network_boot(table.HasKeyInitNetworkBoot());
    characteristics->set_has_targeted_content_distrib(table.HasTargetedContentDistrib());
    characteristics->set_has_uefi(table.HasUEFI());
    characteristics->set_has_virtual_machine(table.HasVirtualMachine());

    return message.SerializeAsString();
}

} // namespace aspia
