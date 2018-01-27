//
// PROJECT:         Aspia
// FILE:            base/devices/physical_drive_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_ENUMERATOR_H
#define _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_ENUMERATOR_H

#include "base/devices/device.h"
#include "base/devices/physical_drive_smart.h"
#include "base/scoped_device_info.h"
#include "category/category_ata.pb.h"

#include <winioctl.h>

namespace aspia {

struct DriveIdentifyData;

class PhysicalDriveEnumerator
{
public:
    PhysicalDriveEnumerator();
    ~PhysicalDriveEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetModelNumber() const;
    std::string GetSerialNumber() const;
    std::string GetFirmwareRevision() const;
    proto::ATA::BusType GetBusType() const;
    proto::ATA::TransferMode GetCurrentTransferMode() const;
    int GetRotationRate() const; // in RPM.
    uint64_t GetDriveSize() const;
    uint32_t GetBufferSize() const; // in bytes.
    int GetMultisectors() const;
    int GetECCSize() const;
    bool IsRemovable() const;
    int64_t GetCylindersNumber() const;
    uint32_t GetTracksPerCylinder() const;
    uint32_t GetSectorsPerTrack() const;
    uint32_t GetBytesPerSector() const;
    uint16_t GetHeadsNumber() const;
    uint64_t GetSupportedFeatures() const;
    uint64_t GetEnabledFeatures() const;
    bool GetSmartData(SmartAttributeData& attributes, SmartThresholdData& thresholds);

private:
    bool GetDriveInfo(uint8_t device_number) const;
    uint16_t GetMajorVersion() const;

    ScopedDeviceInfo device_info_;
    mutable DWORD device_index_ = 0;
    mutable Device device_;
    mutable uint8_t device_number_ = 0;
    mutable std::unique_ptr<DriveIdentifyData> id_data_;
    mutable DISK_GEOMETRY geometry_;

    DISALLOW_COPY_AND_ASSIGN(PhysicalDriveEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__PHYSICAL_DRIVE_ENUMERATOR_H
