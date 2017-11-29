//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/physical_drive_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/physical_drive_enumerator.h"
#include "base/strings/string_util.h"
#include "base/byte_order.h"
#include "base/logging.h"

#define SCSI_IOCTL_DATA_OUT             0
#define SCSI_IOCTL_DATA_IN              1
#define SCSI_IOCTL_DATA_UNSPECIFIED     2

#define IOCTL_SCSI_PASS_THROUGH         0x4D004
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  0x4D014
#define IOCTL_SCSI_GET_ADDRESS          0x41018

#define SPT_SENSEBUFFER_LENGTH          32
#define SPT_DATABUFFER_LENGTH           512

typedef struct
{
    SENDCMDOUTPARAMS send_cmd_out_params;
    BYTE data[IDENTIFY_BUFFER_SIZE - 1];
} READ_IDENTIFY_DATA_OUTDATA, *PREAD_IDENTIFY_DATA_OUTDATA;

typedef struct _SCSI_PASS_THROUGH
{
    USHORT  Length;
    UCHAR  ScsiStatus;
    UCHAR  PathId;
    UCHAR  TargetId;
    UCHAR  Lun;
    UCHAR  CdbLength;
    UCHAR  SenseInfoLength;
    UCHAR  DataIn;
    ULONG  DataTransferLength;
    ULONG  TimeOutValue;
    ULONG_PTR DataBufferOffset;
    ULONG  SenseInfoOffset;
    UCHAR  Cdb[16];
}SCSI_PASS_THROUGH, *PSCSI_PASS_THROUGH;

typedef struct
{
    SCSI_PASS_THROUGH spt;
    ULONG Filler;
    UCHAR SenseBuffer[SPT_SENSEBUFFER_LENGTH];
    UCHAR DataBuffer[SPT_DATABUFFER_LENGTH];
} SCSI_PASS_THROUGH_WBUF;

DEFINE_GUID(GUID_DEVINTERFACE_DISK,
            0x53f56307L, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b);

namespace aspia {

PhysicalDriveEnumerator::PhysicalDriveEnumerator()
{
    memset(&id_data_, 0, sizeof(id_data_));
    memset(&geometry_, 0, sizeof(geometry_));

    const DWORD flags = DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;

    device_info_ = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, flags);
    if (!device_info_ || device_info_ == INVALID_HANDLE_VALUE)
    {
        LOG(WARNING) << "SetupDiGetClassDevsW() failed: " << GetLastSystemErrorString();
    }
}

PhysicalDriveEnumerator::~PhysicalDriveEnumerator()
{
    if (device_info_ && device_info_ != INVALID_HANDLE_VALUE)
    {
        SetupDiDestroyDeviceInfoList(device_info_);
    }
}

bool PhysicalDriveEnumerator::IsAtEnd() const
{
    for (;;)
    {
        SP_DEVICE_INTERFACE_DATA device_iface_data;

        memset(&device_iface_data, 0, sizeof(device_iface_data));
        device_iface_data.cbSize = sizeof(device_iface_data);

        if (!SetupDiEnumDeviceInterfaces(device_info_,
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

        if (SetupDiGetDeviceInterfaceDetailW(device_info_,
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

        if (!SetupDiGetDeviceInterfaceDetailW(device_info_,
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

        if (disk.Open(detail_data->DevicePath,
                      GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE))
        {
            STORAGE_DEVICE_NUMBER device_number;

            if (disk.IoControl(IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &device_number,
                               sizeof(device_number), &required_size))
            {
                if (GetDriveInfo(device_number.DeviceNumber))
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
    ChangeByteOrder(id_data_.model_number, sizeof(id_data_.model_number));
    return id_data_.model_number;
}

std::string PhysicalDriveEnumerator::GetSerialNumber() const
{
    ChangeByteOrder(id_data_.serial_number, sizeof(id_data_.serial_number));
    return id_data_.serial_number;
}

std::string PhysicalDriveEnumerator::GetFirmwareRevision() const
{
    ChangeByteOrder(id_data_.firmware_revision, sizeof(id_data_.firmware_revision));
    return id_data_.firmware_revision;
}

PhysicalDriveEnumerator::BusType PhysicalDriveEnumerator::GetBusType() const
{
    STORAGE_DEVICE_DESCRIPTOR device_descriptor;
    memset(&device_descriptor, 0, sizeof(device_descriptor));

    STORAGE_PROPERTY_QUERY property_query;
    memset(&property_query, 0, sizeof(property_query));

    DWORD bytes_returned;

    if (!device_.IoControl(IOCTL_STORAGE_QUERY_PROPERTY,
                           &property_query, sizeof(property_query),
                           &device_descriptor, sizeof(device_descriptor),
                           &bytes_returned))
    {
        return BusType::UNKNOWN;
    }

    switch (device_descriptor.BusType)
    {
        case BusTypeScsi:
            return BusType::SCSI;

        case BusTypeAtapi:
            return BusType::ATAPI;

        case BusTypeAta:
            return BusType::ATA;

        case BusType1394:
            return BusType::IEEE1394;

        case BusTypeSsa:
            return BusType::SSA;

        case BusTypeFibre:
            return BusType::FIBRE;

        case BusTypeUsb:
            return BusType::USB;

        case BusTypeRAID:
            return BusType::RAID;

        case BusTypeiScsi:
            return BusType::ISCSI;

        case BusTypeSas:
            return BusType::SAS;

        case BusTypeSata:
            return BusType::SATA;

        case BusTypeSd:
            return BusType::SD;

        case BusTypeMmc:
            return BusType::MMC;

        case BusTypeVirtual:
            return BusType::VIRTUAL;

        case BusTypeFileBackedVirtual:
            return BusType::FILE_BACKED_VIRTUAL;

        default:
            return BusType::UNKNOWN;
    }
}

PhysicalDriveEnumerator::TransferMode PhysicalDriveEnumerator::GetCurrentTransferMode() const
{
    TransferMode mode = TransferMode::PIO;

    if (id_data_.multi_word_dma & 0x700)
        mode = TransferMode::PIO_DMA;

    if (id_data_.ultra_dma_mode & 0x4000)
        mode = TransferMode::ULTRA_DMA_133;
    else if (id_data_.ultra_dma_mode & 0x2000)
        mode = TransferMode::ULTRA_DMA_100;
    else if (id_data_.ultra_dma_mode & 0x1000)
        mode = TransferMode::ULTRA_DMA_66;
    else if (id_data_.ultra_dma_mode & 0x800)
        mode = TransferMode::ULTRA_DMA_44;
    else if (id_data_.ultra_dma_mode & 0x400)
        mode = TransferMode::ULTRA_DMA_33;
    else if (id_data_.ultra_dma_mode & 0x200)
        mode = TransferMode::ULTRA_DMA_25;
    else if (id_data_.ultra_dma_mode & 0x100)
        mode = TransferMode::ULTRA_DMA_16;

    if (id_data_.sata_capabilities != 0x0000 && id_data_.sata_capabilities != 0xFFFF)
    {
        if (id_data_.sata_capabilities & 0x10)
            mode = TransferMode::UNKNOWN;
        else if (id_data_.sata_capabilities & 0x8)
            mode = TransferMode::SATA_600;
        else if (id_data_.sata_capabilities & 0x4)
            mode = TransferMode::SATA_300;
        else if (id_data_.sata_capabilities & 0x2)
            mode = TransferMode::SATA_150;
    }

    return mode;
}

int PhysicalDriveEnumerator::GetRotationRate() const
{
    if (id_data_.rotation_rate > 1024 && id_data_.rotation_rate < 65535)
        return id_data_.rotation_rate;

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
    return id_data_.buffer_size * 512;
}

int PhysicalDriveEnumerator::GetMultisectors() const
{
    return id_data_.current_multi_sector_setting;
}

int PhysicalDriveEnumerator::GetECCSize() const
{
    return id_data_.ecc_size;
}

bool PhysicalDriveEnumerator::IsRemovable() const
{
    return id_data_.general_configuration & 0x80;
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
    return id_data_.number_of_heads;
}

uint64_t PhysicalDriveEnumerator::GetSupportedFeatures() const
{
    const uint16_t major_version = GetMajorVersion();

    uint64_t features = 0;

    if (major_version >= 3)
    {
        if (id_data_.command_set_support2 & (1 << 3))
            features |= FEATURE_ADVANCED_POWER_MANAGEMENT;

        if (id_data_.command_set_support1 & (1 << 0))
            features |= FEATURE_SMART;

        if (major_version >= 5)
        {
            if (id_data_.command_set_support2 & (1 << 10))
                features |= FEATURE_48BIT_LBA;

            if (id_data_.command_set_support2 & (1 << 9))
                features |= FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT;

            if (major_version >= 6)
            {
                if (id_data_.sata_capabilities & (1 << 8))
                    features |= FEATURE_NATIVE_COMMAND_QUEUING;

                if (major_version >= 7)
                {
                    if (id_data_.data_set_management & (1 << 0))
                        features |= FEATURE_TRIM;
                }
            }
        }
    }

    if (id_data_.command_set_support3 & (1 << 0))
        features |= FEATURE_SMART_ERROR_LOGGING;

    if (id_data_.command_set_support3 & (1 << 1))
        features |= FEATURE_SMART_SELF_TEST;

    if (id_data_.command_set_support3 & (1 << 4))
        features |= FEATURE_STREAMING;

    if (id_data_.command_set_support3 & (1 << 5))
        features |= FEATURE_GENERAL_PURPOSE_LOGGING;

    if (id_data_.command_set_support1 & (1 << 1))
        features |= FEATURE_SECURITY_MODE;

    if (id_data_.command_set_support1 & (1 << 3))
        features |= FEATURE_POWER_MANAGEMENT;

    if (id_data_.command_set_support1 & (1 << 5))
        features |= FEATURE_WRITE_CACHE;

    if (id_data_.command_set_support1 & (1 << 6))
        features |= FEATURE_READ_LOCK_AHEAD;

    if (id_data_.command_set_support1 & (1 << 7))
        features |= FEATURE_RELEASE_INTERRUPT;

    if (id_data_.command_set_support1 & (1 << 8))
        features |= FEATURE_SERVICE_INTERRUPT;

    if (id_data_.command_set_support1 & (1 << 10))
        features |= FEATURE_HOST_PROTECTED_AREA;

    if (id_data_.command_set_support2 & (1 << 5))
        features |= FEATURE_POWER_UP_IN_STANDBY;

    if (id_data_.command_set_support2 & (1 << 11))
        features |= FEATURE_DEVICE_CONFIGURATION_OVERLAY;

    return features;
}

uint64_t PhysicalDriveEnumerator::GetEnabledFeatures() const
{
    const uint16_t major_version = GetMajorVersion();

    uint64_t features = 0;

    if (major_version >= 3)
    {
        if (id_data_.command_set_support2 & (1 << 3) && id_data_.command_set_enabled2 & (1 << 3))
            features |= FEATURE_ADVANCED_POWER_MANAGEMENT;

        if (id_data_.command_set_support1 & (1 << 0) && id_data_.command_set_enabled1 & (1 << 0))
            features |= FEATURE_SMART;

        if (major_version >= 5)
        {
            if (id_data_.command_set_support2 & (1 << 10) && id_data_.command_set_enabled2 & (1 << 10))
                features |= FEATURE_48BIT_LBA;

            if (id_data_.command_set_support2 & (1 << 9) && id_data_.command_set_enabled2 & (1 << 9))
                features |= FEATURE_AUTOMATIC_ACOUSTIC_MANAGEMENT;

            if (major_version >= 6)
            {
                if (id_data_.sata_capabilities & (1 << 8))
                    features |= FEATURE_NATIVE_COMMAND_QUEUING;

                if (major_version >= 7)
                {
                    if (id_data_.data_set_management & (1 << 0))
                        features |= FEATURE_TRIM;
                }
            }
        }
    }

    if (id_data_.command_set_support3 & (1 << 0))
        features |= FEATURE_SMART_ERROR_LOGGING;

    if (id_data_.command_set_support3 & (1 << 1))
        features |= FEATURE_SMART_SELF_TEST;

    if (id_data_.command_set_support3 & (1 << 4))
        features |= FEATURE_STREAMING;

    if (id_data_.command_set_support3 & (1 << 5))
        features |= FEATURE_GENERAL_PURPOSE_LOGGING;

    if (id_data_.command_set_support1 & (1 << 1) && id_data_.command_set_enabled1 & (1 << 1))
        features |= FEATURE_SECURITY_MODE;

    if (id_data_.command_set_support1 & (1 << 3) && id_data_.command_set_enabled1 & (1 << 3))
        features |= FEATURE_POWER_MANAGEMENT;

    if (id_data_.command_set_support1 & (1 << 5) && id_data_.command_set_enabled1 & (1 << 5))
        features |= FEATURE_WRITE_CACHE;

    if (id_data_.command_set_support1 & (1 << 6) && id_data_.command_set_enabled1 & (1 << 6))
        features |= FEATURE_READ_LOCK_AHEAD;

    if (id_data_.command_set_support1 & (1 << 7) && id_data_.command_set_enabled1 & (1 << 7))
        features |= FEATURE_RELEASE_INTERRUPT;

    if (id_data_.command_set_support1 & (1 << 8) && id_data_.command_set_enabled1 & (1 << 8))
        features |= FEATURE_SERVICE_INTERRUPT;

    if (id_data_.command_set_support1 & (1 << 10) && id_data_.command_set_enabled1 & (1 << 10))
        features |= FEATURE_HOST_PROTECTED_AREA;

    if (id_data_.command_set_support2 & (1 << 5) && id_data_.command_set_enabled2 & (1 << 5))
        features |= FEATURE_POWER_UP_IN_STANDBY;

    if (id_data_.command_set_support2 & (1 << 11) && id_data_.command_set_enabled2 & (1 << 11))
        features |= FEATURE_DEVICE_CONFIGURATION_OVERLAY;

    return features;
}

bool PhysicalDriveEnumerator::GetDriveInfo(DWORD device_number) const
{
    memset(&geometry_, 0, sizeof(geometry_));
    memset(&id_data_, 0, sizeof(id_data_));

    const std::wstring device_path =
        StringPrintfW(L"\\\\.\\PhysicalDrive%lu", device_number);

    if (!device_.Open(device_path))
        return false;

    SENDCMDINPARAMS in;
    memset(&in, 0, sizeof(in));

    in.cBufferSize                  = IDENTIFY_BUFFER_SIZE;
    in.irDriveRegs.bSectorCountReg  = 1;
    in.irDriveRegs.bSectorNumberReg = 1;
    in.irDriveRegs.bDriveHeadReg    = 0xA0 | ((device_number & 1) << 4);
    in.irDriveRegs.bCommandReg      = ID_CMD;
    in.bDriveNumber                 = static_cast<BYTE>(device_number);

    READ_IDENTIFY_DATA_OUTDATA out;
    memset(&out, 0, sizeof(out));

    DWORD bytes_returned;

    if (!device_.IoControl(SMART_RCV_DRIVE_DATA, &in, sizeof(in),
                           &out, sizeof(out), &bytes_returned) ||
        bytes_returned != sizeof(out))
    {
        SCSI_PASS_THROUGH_WBUF scsi_cmd;
        memset(&scsi_cmd, 0, sizeof(scsi_cmd));

        scsi_cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
        scsi_cmd.spt.PathId             = 0;
        scsi_cmd.spt.TargetId           = 1;
        scsi_cmd.spt.Lun                = 0;
        scsi_cmd.spt.TimeOutValue       = 5;
        scsi_cmd.spt.SenseInfoLength    = SPT_SENSEBUFFER_LENGTH;
        scsi_cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, SenseBuffer);
        scsi_cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
        scsi_cmd.spt.DataTransferLength = IDENTIFY_BUFFER_SIZE;
        scsi_cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer);

        scsi_cmd.spt.CdbLength = 12;
        scsi_cmd.spt.Cdb[0]    = 0xA1;
        scsi_cmd.spt.Cdb[1]    = (4 << 1) | 0;
        scsi_cmd.spt.Cdb[2]    = (1 << 3) | (1 << 2) | 2;
        scsi_cmd.spt.Cdb[3]    = 0;
        scsi_cmd.spt.Cdb[4]    = 1;
        scsi_cmd.spt.Cdb[5]    = 0;
        scsi_cmd.spt.Cdb[6]    = 0;
        scsi_cmd.spt.Cdb[7]    = 0;
        scsi_cmd.spt.Cdb[8]    = 0xA0;
        scsi_cmd.spt.Cdb[9]    = ID_CMD;

        if (!device_.IoControl(IOCTL_SCSI_PASS_THROUGH,
                               &scsi_cmd, scsi_cmd.spt.Length,
                               &scsi_cmd, sizeof(scsi_cmd),
                               &bytes_returned))
        {
            return false;
        }
    }

    memcpy(&id_data_, out.send_cmd_out_params.bBuffer, sizeof(id_data_));

    device_.IoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                      nullptr, 0, &geometry_, sizeof(geometry_),
                      &bytes_returned);

    return true;
}

uint16_t PhysicalDriveEnumerator::GetMajorVersion() const
{
    uint16_t major_version = 0;

    if (id_data_.major_version != 0xFFFF && id_data_.major_version != 0)
    {
        uint16_t i = 14;

        while (i > 0)
        {
            if ((id_data_.major_version >> i) & 0x1)
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
