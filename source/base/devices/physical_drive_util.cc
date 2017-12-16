//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/physical_drive_util.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/physical_drive_util.h"
#include "base/strings/string_printf.h"
#include "base/logging.h"

namespace aspia {

namespace {

constexpr BYTE SCSI_IOCTL_DATA_OUT = 0;
constexpr BYTE SCSI_IOCTL_DATA_IN = 1;
constexpr BYTE SCSI_IOCTL_DATA_UNSPECIFIED = 2;

constexpr DWORD IOCTL_SCSI_PASS_THROUGH = 0x4D004;
constexpr DWORD IOCTL_SCSI_PASS_THROUGH_DIRECT = 0x4D014;
constexpr DWORD IOCTL_SCSI_GET_ADDRESS = 0x41018;

constexpr DWORD SPT_SENSEBUFFER_LENGTH = 32;
constexpr DWORD SPT_DATABUFFER_LENGTH = 512;
constexpr BYTE DRIVE_HEAD_REG = 0xA0;

typedef struct
{
    SENDCMDOUTPARAMS send_cmd_out_params;
    BYTE data[IDENTIFY_BUFFER_SIZE - 1];
} READ_IDENTIFY_DATA_OUTDATA;

typedef struct
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
} SCSI_PASS_THROUGH;

typedef struct
{
    SCSI_PASS_THROUGH spt;
    ULONG Filler;
    UCHAR SenseBuffer[SPT_SENSEBUFFER_LENGTH];
    UCHAR DataBuffer[SPT_DATABUFFER_LENGTH];
} SCSI_PASS_THROUGH_WBUF;

} // namespace

bool OpenDrive(Device& device, uint8_t device_number)
{
    const FilePath device_path =
        StringPrintf(L"\\\\.\\PhysicalDrive%u", device_number);

    if (!device.Open(device_path))
    {
        LOG(WARNING) << "Unable to open device: " << device_path
                     << ". Error code: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

STORAGE_BUS_TYPE GetDriveBusType(Device& device)
{
    STORAGE_DEVICE_DESCRIPTOR device_descriptor;
    memset(&device_descriptor, 0, sizeof(device_descriptor));

    STORAGE_PROPERTY_QUERY property_query;
    memset(&property_query, 0, sizeof(property_query));

    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_STORAGE_QUERY_PROPERTY,
                          &property_query, sizeof(property_query),
                          &device_descriptor, sizeof(device_descriptor),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return BusTypeUnknown;
    }

    return device_descriptor.BusType;
}

bool GetDriveNumber(Device& device, uint8_t& device_number)
{
    STORAGE_DEVICE_NUMBER number;
    memset(&number, 0, sizeof(number));

    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_STORAGE_GET_DEVICE_NUMBER,
                          nullptr, 0,
                          &number, sizeof(number),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return false;
    }

    device_number = static_cast<uint8_t>(number.DeviceNumber);
    return true;
}

bool GetDriveGeometry(Device& device, DISK_GEOMETRY& geometry)
{
    DWORD bytes_returned;

    if (device.IoControl(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         nullptr, 0,
                         &geometry, sizeof(DISK_GEOMETRY),
                         &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

std::unique_ptr<DriveIdentifyData>
    GetDriveIdentifyDataPD(Device& device, uint8_t device_number)
{
    SENDCMDINPARAMS cmd_in;
    memset(&cmd_in, 0, sizeof(cmd_in));

    cmd_in.cBufferSize                  = IDENTIFY_BUFFER_SIZE;
    cmd_in.irDriveRegs.bSectorCountReg  = 1;
    cmd_in.irDriveRegs.bSectorNumberReg = 1;
    cmd_in.irDriveRegs.bDriveHeadReg    = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    cmd_in.irDriveRegs.bCommandReg      = ID_CMD;
    cmd_in.bDriveNumber                 = device_number;

    READ_IDENTIFY_DATA_OUTDATA cmd_out;
    memset(&cmd_out, 0, sizeof(cmd_out));

    DWORD bytes_returned;

    if (!device.IoControl(SMART_RCV_DRIVE_DATA,
                          &cmd_in, sizeof(cmd_in),
                          &cmd_out, sizeof(cmd_out),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    std::unique_ptr<DriveIdentifyData> data = std::make_unique<DriveIdentifyData>();
    memcpy(data.get(), cmd_out.send_cmd_out_params.bBuffer, sizeof(DriveIdentifyData));

    return data;
}

bool EnableSmartPD(Device& device, uint8_t device_number)
{
    SENDCMDINPARAMS cmd_in;
    memset(&cmd_in, 0, sizeof(cmd_in));

    cmd_in.cBufferSize                  = 0;
    cmd_in.irDriveRegs.bFeaturesReg     = ENABLE_SMART;
    cmd_in.irDriveRegs.bSectorCountReg  = 1;
    cmd_in.irDriveRegs.bSectorNumberReg = 1;
    cmd_in.irDriveRegs.bCylLowReg       = SMART_CYL_LOW;
    cmd_in.irDriveRegs.bCylHighReg      = SMART_CYL_HI;
    cmd_in.irDriveRegs.bDriveHeadReg    = DRIVE_HEAD_REG;
    cmd_in.irDriveRegs.bCommandReg      = SMART_CMD;
    cmd_in.bDriveNumber                 = device_number;

    SENDCMDOUTPARAMS cmd_out;
    memset(&cmd_out, 0, sizeof(cmd_out));

    DWORD bytes_returned;

    if (!device.IoControl(SMART_SEND_DRIVE_COMMAND,
                          &cmd_in, sizeof(cmd_in),
                          &cmd_out, sizeof(cmd_out),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

std::unique_ptr<DriveIdentifyData>
    GetDriveIdentifyDataSAT(Device& device, uint8_t device_number)
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

    scsi_cmd.spt.CdbLength = 12;  // ATA Pass Through
    scsi_cmd.spt.Cdb[0] = 0xA1;   // Operation Code
    scsi_cmd.spt.Cdb[1] = (4 << 1) | 0;
    scsi_cmd.spt.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    scsi_cmd.spt.Cdb[3] = 0;      // Features
    scsi_cmd.spt.Cdb[4] = 1;      // Sector Count
    scsi_cmd.spt.Cdb[5] = 0;      // LBA_LOW
    scsi_cmd.spt.Cdb[6] = 0;      // LBA_MIDLE
    scsi_cmd.spt.Cdb[7] = 0;      // LBA_HIGH
    scsi_cmd.spt.Cdb[8] = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    scsi_cmd.spt.Cdb[9] = ID_CMD; // Command

    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_SCSI_PASS_THROUGH,
                          &scsi_cmd, scsi_cmd.spt.Length,
                          &scsi_cmd, sizeof(scsi_cmd),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    std::unique_ptr<DriveIdentifyData> data = std::make_unique<DriveIdentifyData>();
    memcpy(data.get(), scsi_cmd.DataBuffer, sizeof(DriveIdentifyData));

    return data;
}

bool EnableSmartSAT(Device& device, uint8_t device_number)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length           = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId           = 0;
    cmd.spt.TargetId         = 0;
    cmd.spt.Lun              = 0;
    cmd.spt.TimeOutValue     = 5;
    cmd.spt.SenseInfoLength  = SPT_SENSEBUFFER_LENGTH;
    cmd.spt.SenseInfoOffset  = offsetof(SCSI_PASS_THROUGH_WBUF, SenseBuffer);
    cmd.spt.DataIn           = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer);

    cmd.spt.CdbLength = 12;
    cmd.spt.Cdb[0] = 0xA1;
    cmd.spt.Cdb[1] = (4 << 1) | 0;
    cmd.spt.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    cmd.spt.Cdb[3] = ENABLE_SMART;
    cmd.spt.Cdb[4] = 1;
    cmd.spt.Cdb[5] = 1;
    cmd.spt.Cdb[6] = SMART_CYL_LOW;
    cmd.spt.Cdb[7] = SMART_CYL_HI;
    cmd.spt.Cdb[8] = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    cmd.spt.Cdb[9] = SMART_CMD;

    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_SCSI_PASS_THROUGH,
                          &cmd, sizeof(SCSI_PASS_THROUGH),
                          &cmd, sizeof(SCSI_PASS_THROUGH_WBUF),
                          &bytes_returned))
    {
        LOG(WARNING) << "IoControl() failed: " << GetLastSystemErrorString();
        return false;
    }

    return true;
}

} // namespace aspia
