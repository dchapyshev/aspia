//
// PROJECT:         Aspia
// FILE:            base/devices/physical_drive_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/physical_drive_enumerator.h"
#include "base/devices/physical_drive_util.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/byte_order.h"
#include "base/logging.h"

DEFINE_GUID(GUID_DEVINTERFACE_DISK,
            0x53f56307L, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

namespace aspia {

PhysicalDriveEnumerator::PhysicalDriveEnumerator()
{
    memset(&geometry_, 0, sizeof(geometry_));

    const DWORD flags = DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;

    device_info_.Reset(SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, flags));
    if (!device_info_.IsValid())
    {
        LOG(WARNING) << "SetupDiGetClassDevsW() failed: " << GetLastSystemErrorString();
    }
}

PhysicalDriveEnumerator::~PhysicalDriveEnumerator() = default;

bool PhysicalDriveEnumerator::IsAtEnd() const
{
    for (;;)
    {
        SP_DEVICE_INTERFACE_DATA device_iface_data;

        memset(&device_iface_data, 0, sizeof(device_iface_data));
        device_iface_data.cbSize = sizeof(device_iface_data);

        if (!SetupDiEnumDeviceInterfaces(device_info_.Get(),
                                         nullptr,
                                         &GUID_DEVINTERFACE_DISK,
                                         device_index_,
                                         &device_iface_data))
        {
            SystemErrorCode error_code = GetLastError();

            if (error_code != ERROR_NO_MORE_ITEMS)
            {
                LOG(WARNING) << "SetupDiEnumDeviceInfo() failed: "
                             << SystemErrorCodeToString(error_code);
            }

            return true;
        }

        DWORD required_size = 0;

        if (SetupDiGetDeviceInterfaceDetailW(device_info_.Get(),
                                             &device_iface_data,
                                             nullptr,
                                             0,
                                             &required_size,
                                             nullptr) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            LOG(WARNING) << "Unexpected return value: " << GetLastSystemErrorString();
            ++device_index_;
            continue;
        }

        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(required_size);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail_data =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.get());

        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(device_info_.Get(),
                                              &device_iface_data,
                                              detail_data,
                                              required_size,
                                              &required_size,
                                              nullptr))
        {
            LOG(WARNING) << "SetupDiGetDeviceInterfaceDetailW() failed: "
                         << GetLastSystemErrorString();
            ++device_index_;
            continue;
        }

        Device disk;

        if (disk.Open(detail_data->DevicePath, GENERIC_READ, FILE_SHARE_READ))
        {
            if (GetDriveNumber(disk, device_number_))
            {
                if (GetDriveInfo(device_number_))
                    break;
            }
        }

        ++device_index_;
    }

    return false;
}

void PhysicalDriveEnumerator::Advance()
{
    ++device_index_;
}

std::string PhysicalDriveEnumerator::GetModelNumber() const
{
    if (!id_data_)
        return std::string();

    std::string output;
    TrimWhitespaceASCII(id_data_->model_number, TRIM_ALL, output);

    return output;
}

std::string PhysicalDriveEnumerator::GetSerialNumber() const
{
    if (!id_data_)
        return std::string();

    std::string output;
    TrimWhitespaceASCII(id_data_->serial_number, TRIM_ALL, output);

    return output;
}

std::string PhysicalDriveEnumerator::GetFirmwareRevision() const
{
    if (!id_data_)
        return std::string();

    std::string output;
    TrimWhitespaceASCII(id_data_->firmware_revision, TRIM_ALL, output);

    return output;
}

proto::ATA::BusType PhysicalDriveEnumerator::GetBusType() const
{
    switch (GetDriveBusType(device_))
    {
        case BusTypeScsi: return proto::ATA::BUS_TYPE_SCSI;
        case BusTypeAtapi: return proto::ATA::BUS_TYPE_ATAPI;
        case BusTypeAta: return proto::ATA::BUS_TYPE_ATA;
        case BusType1394: return proto::ATA::BUS_TYPE_IEEE1394;
        case BusTypeSsa: return proto::ATA::BUS_TYPE_SSA;
        case BusTypeFibre: return proto::ATA::BUS_TYPE_FIBRE;
        case BusTypeUsb: return proto::ATA::BUS_TYPE_USB;
        case BusTypeRAID: return proto::ATA::BUS_TYPE_RAID;
        case BusTypeiScsi: return proto::ATA::BUS_TYPE_ISCSI;
        case BusTypeSas: return proto::ATA::BUS_TYPE_SAS;
        case BusTypeSata: return proto::ATA::BUS_TYPE_SATA;
        case BusTypeSd: return proto::ATA::BUS_TYPE_SD;
        case BusTypeMmc: return proto::ATA::BUS_TYPE_MMC;
        case BusTypeVirtual: return proto::ATA::BUS_TYPE_VIRTUAL;
        case BusTypeFileBackedVirtual: return proto::ATA::BUS_TYPE_FILE_BACKED_VIRTUAL;
        default: return proto::ATA::BUS_TYPE_UNKNOWN;
    }
}

proto::ATA::TransferMode PhysicalDriveEnumerator::GetCurrentTransferMode() const
{
    if (!id_data_)
        return proto::ATA::TRANSFER_MODE_UNKNOWN;

    proto::ATA::TransferMode mode = proto::ATA::TRANSFER_MODE_PIO;

    if (id_data_->multi_word_dma & 0x700)
        mode = proto::ATA::TRANSFER_MODE_PIO_DMA;

    if (id_data_->ultra_dma_mode & 0x4000)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_133;
    else if (id_data_->ultra_dma_mode & 0x2000)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_100;
    else if (id_data_->ultra_dma_mode & 0x1000)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_66;
    else if (id_data_->ultra_dma_mode & 0x800)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_44;
    else if (id_data_->ultra_dma_mode & 0x400)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_33;
    else if (id_data_->ultra_dma_mode & 0x200)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_25;
    else if (id_data_->ultra_dma_mode & 0x100)
        mode = proto::ATA::TRANSFER_MODE_ULTRA_DMA_16;

    if (id_data_->sata_capabilities != 0x0000 && id_data_->sata_capabilities != 0xFFFF)
    {
        if (id_data_->sata_capabilities & 0x10)
            mode = proto::ATA::TRANSFER_MODE_UNKNOWN;
        else if (id_data_->sata_capabilities & 0x8)
            mode = proto::ATA::TRANSFER_MODE_SATA_600;
        else if (id_data_->sata_capabilities & 0x4)
            mode = proto::ATA::TRANSFER_MODE_SATA_300;
        else if (id_data_->sata_capabilities & 0x2)
            mode = proto::ATA::TRANSFER_MODE_SATA_150;
    }

    return mode;
}

int PhysicalDriveEnumerator::GetRotationRate() const
{
    if (id_data_ && id_data_->rotation_rate > 1024 && id_data_->rotation_rate < 65535)
        return id_data_->rotation_rate;

    return 0;
}

uint64_t PhysicalDriveEnumerator::GetDriveSize() const
{
    return geometry_.Cylinders.QuadPart *
        static_cast<int64_t>(geometry_.TracksPerCylinder) *
        static_cast<int64_t>(geometry_.SectorsPerTrack) *
        static_cast<int64_t>(geometry_.BytesPerSector);
}

uint32_t PhysicalDriveEnumerator::GetBufferSize() const
{
    if (!id_data_)
        return 0;

    return id_data_->buffer_size * 512;
}

int PhysicalDriveEnumerator::GetMultisectors() const
{
    if (!id_data_)
        return 0;

    return id_data_->current_multi_sector_setting;
}

int PhysicalDriveEnumerator::GetECCSize() const
{
    if (!id_data_)
        return 0;

    return id_data_->ecc_size;
}

bool PhysicalDriveEnumerator::IsRemovable() const
{
    if (!id_data_)
        return false;

    return id_data_->general_configuration & 0x80;
}

int64_t PhysicalDriveEnumerator::GetCylindersNumber() const
{
    return geometry_.Cylinders.QuadPart;
}

uint32_t PhysicalDriveEnumerator::GetTracksPerCylinder() const
{
    return geometry_.TracksPerCylinder;
}

uint32_t PhysicalDriveEnumerator::GetSectorsPerTrack() const
{
    return geometry_.SectorsPerTrack;
}

uint32_t PhysicalDriveEnumerator::GetBytesPerSector() const
{
    return geometry_.BytesPerSector;
}

uint16_t PhysicalDriveEnumerator::GetHeadsNumber() const
{
    if (!id_data_)
        return 0;

    return id_data_->number_of_heads;
}

uint64_t PhysicalDriveEnumerator::GetSupportedFeatures() const
{
    if (!id_data_)
        return 0;

    const uint16_t major_version = GetMajorVersion();

    uint64_t features = 0;

    if (major_version >= 3)
    {
        if (id_data_->command_set_support2 & (1 << 3))
            features |= proto::ATA::FEATURE_ADVANCED_POWER_MANAGEMENT;

        if (id_data_->command_set_support1 & (1 << 0))
            features |= proto::ATA::FEATURE_SMART;

        if (major_version >= 5)
        {
            if (id_data_->command_set_support2 & (1 << 10))
                features |= proto::ATA::FEATURE_48BIT_LBA;

            if (id_data_->command_set_support2 & (1 << 9))
                features |= proto::ATA::FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT;

            if (major_version >= 6)
            {
                if (id_data_->sata_capabilities & (1 << 8))
                    features |= proto::ATA::FEATURE_NATIVE_COMMAND_QUEUING;

                if (major_version >= 7)
                {
                    if (id_data_->data_set_management & (1 << 0))
                        features |= proto::ATA::FEATURE_TRIM;
                }
            }
        }
    }

    if (id_data_->command_set_support3 & (1 << 0))
        features |= proto::ATA::FEATURE_SMART_ERROR_LOGGING;

    if (id_data_->command_set_support3 & (1 << 1))
        features |= proto::ATA::FEATURE_SMART_SELF_TEST;

    if (id_data_->command_set_support3 & (1 << 4))
        features |= proto::ATA::FEATURE_STREAMING;

    if (id_data_->command_set_support3 & (1 << 5))
        features |= proto::ATA::FEATURE_GENERAL_PURPOSE_LOGGING;

    if (id_data_->command_set_support1 & (1 << 1))
        features |= proto::ATA::FEATURE_SECURITY_MODE;

    if (id_data_->command_set_support1 & (1 << 3))
        features |= proto::ATA::FEATURE_POWER_MANAGEMENT;

    if (id_data_->command_set_support1 & (1 << 5))
        features |= proto::ATA::FEATURE_WRITE_CACHE;

    if (id_data_->command_set_support1 & (1 << 6))
        features |= proto::ATA::FEATURE_READ_LOCK_AHEAD;

    if (id_data_->command_set_support1 & (1 << 7))
        features |= proto::ATA::FEATURE_RELEASE_INTERRUPT;

    if (id_data_->command_set_support1 & (1 << 8))
        features |= proto::ATA::FEATURE_SERVICE_INTERRUPT;

    if (id_data_->command_set_support1 & (1 << 10))
        features |= proto::ATA::FEATURE_HOST_PROTECTED_AREA;

    if (id_data_->command_set_support2 & (1 << 5))
        features |= proto::ATA::FEATURE_POWER_UP_IN_STANDBY;

    if (id_data_->command_set_support2 & (1 << 11))
        features |= proto::ATA::FEATURE_DEVICE_CONFIGURATION_OVERLAY;

    return features;
}

uint64_t PhysicalDriveEnumerator::GetEnabledFeatures() const
{
    if (!id_data_)
        return 0;

    const uint16_t major_version = GetMajorVersion();

    uint64_t features = 0;

    if (major_version >= 3)
    {
        if (id_data_->command_set_support2 & (1 << 3) && id_data_->command_set_enabled2 & (1 << 3))
            features |= proto::ATA::FEATURE_ADVANCED_POWER_MANAGEMENT;

        if (id_data_->command_set_support1 & (1 << 0) && id_data_->command_set_enabled1 & (1 << 0))
            features |= proto::ATA::FEATURE_SMART;

        if (major_version >= 5)
        {
            if (id_data_->command_set_support2 & (1 << 10) && id_data_->command_set_enabled2 & (1 << 10))
                features |= proto::ATA::FEATURE_48BIT_LBA;

            if (id_data_->command_set_support2 & (1 << 9) && id_data_->command_set_enabled2 & (1 << 9))
                features |= proto::ATA::FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT;

            if (major_version >= 6)
            {
                if (id_data_->sata_capabilities & (1 << 8))
                    features |= proto::ATA::FEATURE_NATIVE_COMMAND_QUEUING;

                if (major_version >= 7)
                {
                    if (id_data_->data_set_management & (1 << 0))
                        features |= proto::ATA::FEATURE_TRIM;
                }
            }
        }
    }

    if (id_data_->command_set_support3 & (1 << 0))
        features |= proto::ATA::FEATURE_SMART_ERROR_LOGGING;

    if (id_data_->command_set_support3 & (1 << 1))
        features |= proto::ATA::FEATURE_SMART_SELF_TEST;

    if (id_data_->command_set_support3 & (1 << 4))
        features |= proto::ATA::FEATURE_STREAMING;

    if (id_data_->command_set_support3 & (1 << 5))
        features |= proto::ATA::FEATURE_GENERAL_PURPOSE_LOGGING;

    if (id_data_->command_set_support1 & (1 << 1) && id_data_->command_set_enabled1 & (1 << 1))
        features |= proto::ATA::FEATURE_SECURITY_MODE;

    if (id_data_->command_set_support1 & (1 << 3) && id_data_->command_set_enabled1 & (1 << 3))
        features |= proto::ATA::FEATURE_POWER_MANAGEMENT;

    if (id_data_->command_set_support1 & (1 << 5) && id_data_->command_set_enabled1 & (1 << 5))
        features |= proto::ATA::FEATURE_WRITE_CACHE;

    if (id_data_->command_set_support1 & (1 << 6) && id_data_->command_set_enabled1 & (1 << 6))
        features |= proto::ATA::FEATURE_READ_LOCK_AHEAD;

    if (id_data_->command_set_support1 & (1 << 7) && id_data_->command_set_enabled1 & (1 << 7))
        features |= proto::ATA::FEATURE_RELEASE_INTERRUPT;

    if (id_data_->command_set_support1 & (1 << 8) && id_data_->command_set_enabled1 & (1 << 8))
        features |= proto::ATA::FEATURE_SERVICE_INTERRUPT;

    if (id_data_->command_set_support1 & (1 << 10) && id_data_->command_set_enabled1 & (1 << 10))
        features |= proto::ATA::FEATURE_HOST_PROTECTED_AREA;

    if (id_data_->command_set_support2 & (1 << 5) && id_data_->command_set_enabled2 & (1 << 5))
        features |= proto::ATA::FEATURE_POWER_UP_IN_STANDBY;

    if (id_data_->command_set_support2 & (1 << 11) && id_data_->command_set_enabled2 & (1 << 11))
        features |= proto::ATA::FEATURE_DEVICE_CONFIGURATION_OVERLAY;

    return features;
}

bool PhysicalDriveEnumerator::GetSmartData(SmartAttributeData& attributes,
                                           SmartThresholdData& thresholds)
{
    STORAGE_BUS_TYPE bus_type = GetDriveBusType(device_);

    switch (bus_type)
    {
        case BusTypeAta:
        case BusTypeAtapi:
        case BusTypeSata:
        {
            if (!EnableSmartPD(device_, device_number_))
                break;

            if (!GetSmartAttributesPD(device_, device_number_, attributes))
                break;

            if (!GetSmartThresholdsPD(device_, device_number_, thresholds))
                break;

            return true;
        }
        break;

        case BusTypeUsb:
        {
            if (!EnableSmartSAT(device_, device_number_))
                break;

            if (!GetSmartAttributesSAT(device_, device_number_, attributes))
                break;

            if (!GetSmartThresholdsSAT(device_, device_number_, thresholds))
                break;

            return true;
        }
        break;

        default:
            LOG(WARNING) << "Unhandled bus type: " << bus_type;
            break;
    }

    return false;
}

bool PhysicalDriveEnumerator::GetDriveInfo(uint8_t device_number) const
{
    memset(&geometry_, 0, sizeof(geometry_));
    id_data_.reset();

    if (!OpenDrive(device_, device_number))
        return false;

    STORAGE_BUS_TYPE bus_type = GetDriveBusType(device_);

    switch (bus_type)
    {
        case BusTypeAta:
        case BusTypeAtapi:
        case BusTypeSata:
            id_data_ = GetDriveIdentifyDataPD(device_, device_number);
            break;

        case BusTypeUsb:
            id_data_ = GetDriveIdentifyDataSAT(device_, device_number);
            break;

        default:
            LOG(WARNING) << "Unhandled bus type: " << bus_type;
            break;
    }

    if (!id_data_)
        return false;

    ChangeByteOrder(id_data_->model_number, sizeof(id_data_->model_number));
    ChangeByteOrder(id_data_->serial_number, sizeof(id_data_->serial_number));
    ChangeByteOrder(id_data_->firmware_revision, sizeof(id_data_->firmware_revision));

    GetDriveGeometry(device_, geometry_);

    return true;
}

uint16_t PhysicalDriveEnumerator::GetMajorVersion() const
{
    if (!id_data_)
        return 0;

    uint16_t major_version = 0;

    if (id_data_->major_version != 0xFFFF && id_data_->major_version != 0)
    {
        uint16_t i = 14;

        while (i > 0)
        {
            if ((id_data_->major_version >> i) & 0x1)
            {
                major_version = i;
                break;
            }

            --i;
        }
    }

    return major_version;
}

} // namespace aspia
