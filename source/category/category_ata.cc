//
// PROJECT:         Aspia
// FILE:            category/category_ata.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_ata.h"
#include "category/category_ata.pb.h"
#include "base/devices/physical_drive_enumerator.h"
#include "ui/resource.h"

namespace aspia {

namespace {

const char* BusTypeToString(proto::ATA::BusType value)
{
    switch (value)
    {
        case proto::ATA::BUS_TYPE_SCSI:
            return "SCSI";

        case proto::ATA::BUS_TYPE_ATAPI:
            return "ATAPI";

        case proto::ATA::BUS_TYPE_ATA:
            return "ATA";

        case proto::ATA::BUS_TYPE_IEEE1394:
            return "IEEE 1394";

        case proto::ATA::BUS_TYPE_SSA:
            return "SSA";

        case proto::ATA::BUS_TYPE_FIBRE:
            return "Fibre";

        case proto::ATA::BUS_TYPE_USB:
            return "USB";

        case proto::ATA::BUS_TYPE_RAID:
            return "RAID";

        case proto::ATA::BUS_TYPE_ISCSI:
            return "iSCSI";

        case proto::ATA::BUS_TYPE_SAS:
            return "SAS";

        case proto::ATA::BUS_TYPE_SATA:
            return "SATA";

        case proto::ATA::BUS_TYPE_SD:
            return "SD";

        case proto::ATA::BUS_TYPE_MMC:
            return "MMC";

        case proto::ATA::BUS_TYPE_VIRTUAL:
            return "Virtual";

        case proto::ATA::BUS_TYPE_FILE_BACKED_VIRTUAL:
            return "File Backed Virtual";

        default:
            return "Unknown";
    }
}

const char* TransferModeToString(proto::ATA::TransferMode value)
{
    switch (value)
    {
        case proto::ATA::TRANSFER_MODE_PIO:
            return "PIO";

        case proto::ATA::TRANSFER_MODE_PIO_DMA:
            return "PIO / DMA";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_133:
            return "UltraDMA/133 (133 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_100:
            return "UltraDMA/100 (100 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_66:
            return "UltraDMA/66 (66 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_44:
            return "UltraDMA/44 (44 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_33:
            return "UltraDMA/33 (33 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_25:
            return "UltraDMA/25 (25 MB/s)";

        case proto::ATA::TRANSFER_MODE_ULTRA_DMA_16:
            return "UltraDMA/16 (16 MB/s)";

        case proto::ATA::TRANSFER_MODE_SATA_600:
            return "SATA/600 (600 MB/s)";

        case proto::ATA::TRANSFER_MODE_SATA_300:
            return "SATA/300 (300 MB/s)";

        case proto::ATA::TRANSFER_MODE_SATA_150:
            return "SATA/150 (150 MB/s)";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryATA::CategoryATA()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

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
    return "{79D80586-D264-46E6-8718-09E267730B78}";
}

void CategoryATA::Parse(Table& table, const std::string& data)
{
    proto::ATA message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::ATA::Item& item = message.item(index);

        Group group = table.AddGroup(item.model_number());

        if (!item.serial_number().empty())
            group.AddParam("Serial Number", Value::String(item.serial_number()));

        if (!item.firmware_revision().empty())
            group.AddParam("Firmware Revision", Value::String(item.firmware_revision()));

        group.AddParam("Bus Type", Value::String(BusTypeToString(item.bus_type())));
        group.AddParam("Transfer Mode", Value::String(TransferModeToString(item.transfer_mode())));

        if (item.rotation_rate())
            group.AddParam("Rotation Rate", Value::Number(item.rotation_rate(), "RPM"));

        if (item.drive_size())
            group.AddParam("Drive Size", Value::MemorySize(item.drive_size()));

        if (item.buffer_size())
            group.AddParam("Buffer Size", Value::MemorySize(item.buffer_size()));

        if (item.multisectors())
            group.AddParam("Multisectors", Value::Number(item.multisectors()));

        if (item.ecc_size())
            group.AddParam("ECC Size", Value::Number(item.ecc_size()));

        group.AddParam("Removable", Value::Bool(item.is_removable()));

        if (item.heads_number())
            group.AddParam("Heads Count", Value::Number(item.heads_number()));

        if (item.cylinders_number())
            group.AddParam("Cylinders Count", Value::Number(item.cylinders_number()));

        if (item.tracks_per_cylinder())
            group.AddParam("Tracks per Cylinder", Value::Number(item.tracks_per_cylinder()));

        if (item.sectors_per_track())
            group.AddParam("Sectors per Track", Value::Number(item.sectors_per_track()));

        if (item.bytes_per_sector())
            group.AddParam("Bytes per Sector", Value::Number(item.bytes_per_sector()));

        if (item.supported_features())
        {
            Group features_group = group.AddGroup("Features");

            auto add_feature = [&](const char* name, uint64_t flag)
            {
                if (item.supported_features() & flag)
                    features_group.AddParam(name, Value::Bool(item.enabled_features() & flag));
            };

            add_feature("48-bit LBA", proto::ATA::FEATURE_48BIT_LBA);
            add_feature("Advanced Power Management", proto::ATA::FEATURE_ADVANCED_POWER_MANAGEMENT);
            add_feature("Automatic Acoustic Management", proto::ATA::FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT);
            add_feature("Device Configuration Overlay", proto::ATA::FEATURE_DEVICE_CONFIGURATION_OVERLAY);
            add_feature("General Purpose Logging", proto::ATA::FEATURE_GENERAL_PURPOSE_LOGGING);
            add_feature("Host Protected Area", proto::ATA::FEATURE_HOST_PROTECTED_AREA);
            add_feature("Read Lock Ahead", proto::ATA::FEATURE_READ_LOCK_AHEAD);
            add_feature("Write Cache", proto::ATA::FEATURE_WRITE_CACHE);
            add_feature("Native Command Queuing", proto::ATA::FEATURE_NATIVE_COMMAND_QUEUING);
            add_feature("Power Management", proto::ATA::FEATURE_POWER_MANAGEMENT);
            add_feature("Power Up In Standby", proto::ATA::FEATURE_POWER_UP_IN_STANDBY);
            add_feature("Release Interrupt", proto::ATA::FEATURE_RELEASE_INTERRUPT);
            add_feature("Service Interrupt", proto::ATA::FEATURE_SERVICE_INTERRUPT);
            add_feature("Security Mode", proto::ATA::FEATURE_SECURITY_MODE);
            add_feature("Streaming", proto::ATA::FEATURE_STREAMING);
            add_feature("SMART", proto::ATA::FEATURE_SMART);
            add_feature("SMART Error Logging", proto::ATA::FEATURE_SMART_ERROR_LOGGING);
            add_feature("SMART Self Test", proto::ATA::FEATURE_SMART_SELF_TEST);
            add_feature("TRIM", proto::ATA::FEATURE_TRIM);
        }
    }
}

std::string CategoryATA::Serialize()
{
    proto::ATA message;

    for (PhysicalDriveEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::ATA::Item* item = message.add_item();

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
        item->set_supported_features(enumerator.GetSupportedFeatures());
        item->set_enabled_features(enumerator.GetEnabledFeatures());
    }

    return message.SerializeAsString();
}

} // namespace aspia
