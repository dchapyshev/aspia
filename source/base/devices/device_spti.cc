//
// PROJECT:         Aspia
// FILE:            base/devices/device_spti.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_spti.h"

#include <winioctl.h>
#include <ntddscsi.h>

namespace aspia {

namespace {

constexpr uint32_t kSenseBufferLength = 32;
constexpr uint32_t kDataBufferLength = 512;

typedef struct
{
    SCSI_PASS_THROUGH spt;
    uint32_t filler;
    uint8_t sense_buffer[kSenseBufferLength];
    uint8_t data_buffer[kDataBufferLength];
} SCSI_PASS_THROUGH_WBUF;

} // namespace

bool DeviceSPTI::GetInquiryData(InquiryData* inquiry_data)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 1;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.SenseInfoLength    = kSenseBufferLength;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, sense_buffer);
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = sizeof(InquiryData);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, data_buffer);

    cmd.spt.CdbLength = 6;
    cmd.spt.Cdb[0]    = OP_INQUIRY;
    cmd.spt.Cdb[3]    = (cmd.spt.DataTransferLength >> 8) & 0xFF;
    cmd.spt.Cdb[4]    = cmd.spt.DataTransferLength & 0xFF;

    DWORD bytes_returned;

    if (IoControl(IOCTL_SCSI_PASS_THROUGH,
                  &cmd, sizeof(cmd),
                  &cmd, sizeof(cmd),
                  &bytes_returned))
    {
        memcpy(inquiry_data, cmd.data_buffer, sizeof(InquiryData));
        return true;
    }

    return false;
}

bool DeviceSPTI::GetConfiguration(FeatureCode feature_code, Feature* feature)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 1;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.SenseInfoLength    = kSenseBufferLength;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, sense_buffer);
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = sizeof(Feature);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, data_buffer);

    cmd.spt.CdbLength = 10;
    cmd.spt.Cdb[0]    = OP_GET_CONFIGURATION;
    cmd.spt.Cdb[2]    = (feature_code >> 8) & 0xFF;
    cmd.spt.Cdb[3]    = feature_code & 0xFF;
    cmd.spt.Cdb[7]    = (cmd.spt.DataTransferLength >> 8) & 0xFF;
    cmd.spt.Cdb[8]    = cmd.spt.DataTransferLength & 0xFF;

    DWORD bytes_returned;

    if (IoControl(IOCTL_SCSI_PASS_THROUGH,
                  &cmd, sizeof(cmd),
                  &cmd, sizeof(cmd),
                  &bytes_returned))
    {
        memcpy(feature, &cmd.data_buffer[0], sizeof(Feature));
        return true;
    }

    return false;
}

bool DeviceSPTI::GetReportKeyData(ReportKeyData* report_key_data)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 1;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 5;
    cmd.spt.SenseInfoLength    = kSenseBufferLength;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, sense_buffer);
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.DataTransferLength = sizeof(ReportKeyData);
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, data_buffer);

    cmd.spt.CdbLength = 12;
    cmd.spt.Cdb[0]  = OP_REPORT_KEY;
    cmd.spt.Cdb[8]  = (cmd.spt.DataTransferLength >> 8) & 0xFF;
    cmd.spt.Cdb[9]  = cmd.spt.DataTransferLength & 0xFF;
    cmd.spt.Cdb[10] = 8; // KEY_FORMAT=RPC_STATE

    DWORD bytes_returned;

    if (IoControl(IOCTL_SCSI_PASS_THROUGH,
                  &cmd, sizeof(cmd),
                  &cmd, sizeof(cmd),
                  &bytes_returned))
    {
        memcpy(report_key_data, &cmd.data_buffer[0], sizeof(ReportKeyData));
        return true;
    }

    return false;
}

bool DeviceSPTI::GetModeSenseData(uint8_t page, ModeSenseData* mode_sense_data)
{
    SCSI_PASS_THROUGH_WBUF cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    cmd.spt.PathId             = 0;
    cmd.spt.TargetId           = 1;
    cmd.spt.Lun                = 0;
    cmd.spt.TimeOutValue       = 2;
    cmd.spt.DataIn             = SCSI_IOCTL_DATA_IN;
    cmd.spt.SenseInfoLength    = kSenseBufferLength;
    cmd.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_WBUF, sense_buffer);
    cmd.spt.DataTransferLength = kDataBufferLength;
    cmd.spt.DataBufferOffset   = offsetof(SCSI_PASS_THROUGH_WBUF, data_buffer);

    cmd.spt.CdbLength = 10;
    cmd.spt.Cdb[0] = OP_MODE_SENSE_10; // Operation code.
    cmd.spt.Cdb[2] = page; // Page code.
    cmd.spt.Cdb[7] = (cmd.spt.DataTransferLength >> 8) & 0xFF;
    cmd.spt.Cdb[8] = cmd.spt.DataTransferLength & 0xFF;

    DWORD length = cmd.spt.DataBufferOffset + cmd.spt.DataTransferLength;
    DWORD bytes_returned;

    if (IoControl(IOCTL_SCSI_PASS_THROUGH,
                  &cmd, sizeof(SCSI_PASS_THROUGH),
                  &cmd, length,
                  &bytes_returned))
    {
        memcpy(mode_sense_data, &cmd.data_buffer[0], sizeof(ModeSenseData));
        return true;
    }

    return false;
}

} // namespace aspia
