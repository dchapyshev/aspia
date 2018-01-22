//
// PROJECT:         Aspia
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
    SENDCMDOUTPARAMS send_cmd_out_params;
    BYTE data[READ_ATTRIBUTE_BUFFER_SIZE - 1];
} READ_ATTRIBUTE_DATA_OUTDATA;

typedef struct
{
    SENDCMDOUTPARAMS send_cmd_out_params;
    BYTE data[READ_THRESHOLD_BUFFER_SIZE - 1];
} READ_THRESHOLD_DATA_OUTDATA;

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
    const std::experimental::filesystem::path device_path =
        StringPrintf(L"\\\\.\\PhysicalDrive%u", device_number);

    if (!device.Open(device_path))
    {
        DPLOG(LS_WARNING) << "Unable to open device: " << device_path;
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
        DPLOG(LS_WARNING) << "IoControl() failed";
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
        DPLOG(LS_WARNING) << "IoControl() failed";
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
        DPLOG(LS_WARNING) << "IoControl() failed";
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
        DPLOG(LS_WARNING) << "IoControl() failed";
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
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    return true;
}

bool GetSmartAttributesPD(Device& device, uint8_t device_number, SmartAttributeData& data)
{
    SENDCMDINPARAMS cmd_in;
    memset(&cmd_in, 0, sizeof(cmd_in));

    cmd_in.cBufferSize                  = READ_ATTRIBUTE_BUFFER_SIZE;
    cmd_in.irDriveRegs.bFeaturesReg     = READ_ATTRIBUTES;
    cmd_in.irDriveRegs.bSectorCountReg  = 1;
    cmd_in.irDriveRegs.bSectorNumberReg = 1;
    cmd_in.irDriveRegs.bCylLowReg       = SMART_CYL_LOW;
    cmd_in.irDriveRegs.bCylHighReg      = SMART_CYL_HI;
    cmd_in.irDriveRegs.bDriveHeadReg    = DRIVE_HEAD_REG;
    cmd_in.irDriveRegs.bCommandReg      = SMART_CMD;
    cmd_in.bDriveNumber                 = device_number;

    READ_ATTRIBUTE_DATA_OUTDATA cmd_out;
    memset(&cmd_out, 0, sizeof(cmd_out));

    DWORD bytes_returned;

    if (!device.IoControl(SMART_RCV_DRIVE_DATA,
                          &cmd_in, sizeof(cmd_in),
                          &cmd_out, sizeof(cmd_out),
                          &bytes_returned) ||
        bytes_returned != sizeof(cmd_out))
    {
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    memcpy(&data,
           &cmd_out.send_cmd_out_params.bBuffer[0],
           READ_ATTRIBUTE_BUFFER_SIZE);

    return true;
}

bool GetSmartThresholdsPD(Device& device, uint8_t device_number, SmartThresholdData& data)
{
    SENDCMDINPARAMS cmd_in;
    memset(&cmd_in, 0, sizeof(cmd_in));

    cmd_in.cBufferSize                  = READ_THRESHOLD_BUFFER_SIZE;
    cmd_in.irDriveRegs.bFeaturesReg     = READ_THRESHOLDS;
    cmd_in.irDriveRegs.bSectorCountReg  = 1;
    cmd_in.irDriveRegs.bSectorNumberReg = 1;
    cmd_in.irDriveRegs.bCylLowReg       = SMART_CYL_LOW;
    cmd_in.irDriveRegs.bCylHighReg      = SMART_CYL_HI;
    cmd_in.irDriveRegs.bDriveHeadReg    = DRIVE_HEAD_REG;
    cmd_in.irDriveRegs.bCommandReg      = SMART_CMD;
    cmd_in.bDriveNumber                 = device_number;

    READ_THRESHOLD_DATA_OUTDATA cmd_out;
    memset(&cmd_out, 0, sizeof(cmd_out));

    DWORD bytes_returned;

    if (!device.IoControl(SMART_RCV_DRIVE_DATA,
                          &cmd_in, sizeof(cmd_in),
                          &cmd_out, sizeof(cmd_out),
                          &bytes_returned) ||
        bytes_returned != sizeof(cmd_out))
    {
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    memcpy(&data,
           &cmd_out.send_cmd_out_params.bBuffer[0],
           READ_THRESHOLD_BUFFER_SIZE);

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
    // MULTIPLE_COUNT=0, PROTOCOL=4 (PIO Data-In), Reserved
    scsi_cmd.spt.Cdb[1] = (4 << 1) | 0;
    // OFF_LINE=0, CK_COND=0, Reserved=0, T_DIR=1 (ToDevice), BYTE_BLOCK=1, T_LENGTH=2
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
        DPLOG(LS_WARNING) << "IoControl() failed";
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

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 0;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = 0;
    cmd.spt.SenseInfoLength    = SPT_SENSEBUFFER_LENGTH;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, SenseBuffer);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer);

    cmd.spt.CdbLength = 12;         // ATA Pass Through
    cmd.spt.Cdb[0] = 0xA1;          // Operation Code
    // MULTIPLE_COUNT=0, PROTOCOL=3 (Non-Data), Reserved
    cmd.spt.Cdb[1] = (3 << 1) | 0;
    // OFF_LINE=0, CK_COND=0, Reserved=0, T_DIR=1 (ToDevice), BYTE_BLOCK=1, T_LENGTH=2
    cmd.spt.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    cmd.spt.Cdb[3] = ENABLE_SMART;  // Features
    cmd.spt.Cdb[4] = 0;             // Sector Count
    cmd.spt.Cdb[5] = 1;             // LBA_LOW
    cmd.spt.Cdb[6] = SMART_CYL_LOW; // LBA_MIDLE
    cmd.spt.Cdb[7] = SMART_CYL_HI;  // LBA_HIGH
    cmd.spt.Cdb[8] = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    cmd.spt.Cdb[9] = SMART_CMD;     // Command

    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_SCSI_PASS_THROUGH,
                          &cmd, sizeof(SCSI_PASS_THROUGH),
                          &cmd, sizeof(SCSI_PASS_THROUGH_WBUF),
                          &bytes_returned))
    {
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    return true;
}

bool GetSmartAttributesSAT(Device& device, uint8_t device_number, SmartAttributeData& data)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 0;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = READ_ATTRIBUTE_BUFFER_SIZE;
    cmd.spt.SenseInfoLength    = SPT_SENSEBUFFER_LENGTH;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, SenseBuffer);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer);

    cmd.spt.CdbLength = 12;           // ATA Pass Through
    cmd.spt.Cdb[0] = 0xA1;            // Operation Code
    // MULTIPLE_COUNT=0, PROTOCOL=4 (PIO Data-In), Reserved
    cmd.spt.Cdb[1] = (4 << 1) | 0;
    // OFF_LINE=0, CK_COND=0, Reserved=0, T_DIR=1 (ToDevice), BYTE_BLOCK=1, T_LENGTH=2
    cmd.spt.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    cmd.spt.Cdb[3] = READ_ATTRIBUTES; // Features
    cmd.spt.Cdb[4] = 1;               // Sector Count
    cmd.spt.Cdb[5] = 1;               // LBA_LOW
    cmd.spt.Cdb[6] = SMART_CYL_LOW;   // LBA_MIDLE
    cmd.spt.Cdb[7] = SMART_CYL_HI;    // LBA_HIGH
    cmd.spt.Cdb[8] = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    cmd.spt.Cdb[9] = SMART_CMD;       // Command

    DWORD length = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer) + cmd.spt.DataTransferLength;
    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_SCSI_PASS_THROUGH,
                          &cmd, sizeof(SCSI_PASS_THROUGH),
                          &cmd, length,
                          &bytes_returned) ||
        bytes_returned != length)
    {
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    memcpy(&data, &cmd.DataBuffer[0], READ_ATTRIBUTE_BUFFER_SIZE);

    return true;
}

bool GetSmartThresholdsSAT(Device& device, uint8_t device_number, SmartThresholdData& data)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 0;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = READ_THRESHOLD_BUFFER_SIZE;
    cmd.spt.SenseInfoLength    = SPT_SENSEBUFFER_LENGTH;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, SenseBuffer);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer);

    cmd.spt.CdbLength = 12;           // ATA Pass Through
    cmd.spt.Cdb[0] = 0xA1;            // Operation Code
    // MULTIPLE_COUNT=0, PROTOCOL=4 (PIO Data-In), Reserved
    cmd.spt.Cdb[1] = (4 << 1) | 0;
    // OFF_LINE=0, CK_COND=0, Reserved=0, T_DIR=1 (ToDevice), BYTE_BLOCK=1, T_LENGTH=2
    cmd.spt.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    cmd.spt.Cdb[3] = READ_THRESHOLDS; // Features
    cmd.spt.Cdb[4] = 1;               // Sector Count
    cmd.spt.Cdb[5] = 1;               // LBA_LOW
    cmd.spt.Cdb[6] = SMART_CYL_LOW;   // LBA_MIDLE
    cmd.spt.Cdb[7] = SMART_CYL_HI;    // LBA_HIGH
    cmd.spt.Cdb[8] = DRIVE_HEAD_REG | ((device_number & 1) << 4);
    cmd.spt.Cdb[9] = SMART_CMD;       // Command

    DWORD length = offsetof(SCSI_PASS_THROUGH_WBUF, DataBuffer) + cmd.spt.DataTransferLength;
    DWORD bytes_returned;

    if (!device.IoControl(IOCTL_SCSI_PASS_THROUGH,
                          &cmd, sizeof(SCSI_PASS_THROUGH),
                          &cmd, length,
                          &bytes_returned) ||
        bytes_returned != length)
    {
        DPLOG(LS_WARNING) << "IoControl() failed";
        return false;
    }

    memcpy(&data, &cmd.DataBuffer[0], READ_THRESHOLD_BUFFER_SIZE);

    return true;
}

} // namespace aspia
