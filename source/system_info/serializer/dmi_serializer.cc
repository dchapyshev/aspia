//
// PROJECT:         Aspia
// FILE:            system_info/serializer/dmi_serializer.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/serializer/dmi_serializer.h"

#include "system_info/protocol/dmi.pb.h"
#include "system_info/serializer/dmi_impl.h"

namespace aspia {

namespace {

void addBiosTable(system_info::dmi::Bios* bios, const DmiBiosTable* table)
{
    bios->set_manufacturer(table->manufacturer().toStdString());
    bios->set_version(table->version().toStdString());
    bios->set_date(table->date().toStdString());
    bios->set_size(table->biosSize());
    bios->set_bios_revision(table->biosRevision().toStdString());
    bios->set_firmware_revision(table->firmwareRevision().toStdString());
    bios->set_address(table->address().toStdString());
    bios->set_runtime_size(table->runtimeSize());

    DmiBiosTable::Characteristics result;
    table->characteristics(&result);

    system_info::dmi::Bios::Characteristics* ch = bios->mutable_characteristics();

    ch->set_isa(result.isa);
    ch->set_mca(result.mca);
    ch->set_eisa(result.eisa);
    ch->set_pci(result.pci);
    ch->set_pc_card(result.pc_card);
    ch->set_pnp(result.pnp);
    ch->set_apm(result.apm);
    ch->set_bios_upgradeable(result.bios_upgradeable);
    ch->set_bios_shadowing(result.bios_shadowing);
    ch->set_vlb(result.vlb);
    ch->set_escd(result.escd);
    ch->set_boot_from_cd(result.boot_from_cd);
    ch->set_selectable_boot(result.selectable_boot);
    ch->set_socketed_boot_rom(result.socketed_boot_rom);
    ch->set_boot_from_pc_card(result.boot_from_pc_card);
    ch->set_edd(result.edd);
    ch->set_japanese_floppy_for_nec9800(result.japanese_floppy_for_nec9800);
    ch->set_japanese_floppy_for_toshiba(result.japanese_floppy_for_toshiba);
    ch->set_floppy_525_360kb(result.floppy_525_360kb);
    ch->set_floppy_525_12mb(result.floppy_525_12mb);
    ch->set_floppy_35_720kb(result.floppy_35_720kb);
    ch->set_floppy_35_288mb(result.floppy_35_288mb);
    ch->set_print_screen(result.print_screen);
    ch->set_keyboard_8042(result.keyboard_8042);
    ch->set_serial(result.serial);
    ch->set_printer(result.printer);
    ch->set_cga_video(result.cga_video);
    ch->set_nec_pc98(result.nec_pc98);
    ch->set_acpi(result.acpi);
    ch->set_usb_legacy(result.usb_legacy);
    ch->set_agp(result.agp);
    ch->set_i2o_boot(result.i2o_boot);
    ch->set_ls120_boot(result.ls120_boot);
    ch->set_atapi_zip_drive_boot(result.atapi_zip_drive_boot);
    ch->set_ieee1394_boot(result.ieee1394_boot);
    ch->set_smart_battery(result.smart_battery);
    ch->set_bios_boot_specification(result.bios_boot_specification);
    ch->set_key_init_network_boot(result.key_init_network_boot);
    ch->set_targeted_content_distrib(result.targeted_content_distrib);
    ch->set_uefi(result.uefi);
    ch->set_virtual_machine(result.virtual_machine);
}

} // namespace

DmiSerializer::DmiSerializer(QObject* parent, const QString& uuid)
    : Serializer(parent, uuid)
{
    // Nothing
}

// static
Serializer* DmiSerializer::create(QObject* parent, const QString& uuid)
{
    return new DmiSerializer(parent, uuid);
}

void DmiSerializer::start()
{
    system_info::dmi::Dmi dmi;

    for (DmiTableEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        const DmiTable* table = enumerator.table();
        if (!table)
            continue;

        switch (table->type())
        {
            case DmiTable::TYPE_BIOS:
                addBiosTable(dmi.add_bios(), reinterpret_cast<const DmiBiosTable*>(table));
                break;
        }
    }

    onReady(dmi);
}

} // namespace aspia
