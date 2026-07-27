//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_PHYSICAL_DRIVE_READER_MAC_H
#define BASE_PHYSICAL_DRIVE_READER_MAC_H

#include "base/physical_drive_reader.h"

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/ata/ATASMARTLib.h>
#include <IOKit/storage/nvme/NVMeSMARTLibExternal.h>

class PhysicalDriveReaderMac final : public PhysicalDriveReader
{
public:
    PhysicalDriveReaderMac() = default;
    ~PhysicalDriveReaderMac() final;

    // Paths of the physical drives present in the system.
    static QStringList devicePaths();

    // PhysicalDriveReader implementation.
    QString model() const final { return model_; }
    QString serialNumber() const final { return serial_number_; }
    QString firmwareRevision() const final { return firmware_revision_; }
    BusType busType() const final { return bus_type_; }
    quint64 size() const final { return size_; }
    bool isRemovable() const final { return removable_; }
    bool isSolidState() const final { return solid_state_; }
    QByteArray ataIdentifyData() final;
    QByteArray ataSmartAttributes() final;
    QByteArray ataSmartThresholds() final;
    QByteArray nvmeHealthLog() final;

protected:
    // PhysicalDriveReader implementation.
    bool open(const QString& device_path) final;

private:
    // The media holds what the operating system knows about the medium, the device behind it what
    // the driver knows about the drive itself.
    void readMedia(io_service_t media);
    void readIdentity(io_service_t device);

    // Creates the S.M.A.R.T. user client the drive publishes, if it publishes one. Every health
    // request of this class goes through it.
    void openSmartInterface(io_service_t device);
    bool enableAtaSmart();

    QString model_;
    QString serial_number_;
    QString firmware_revision_;
    BusType bus_type_ = BusType::UNKNOWN;
    quint64 size_ = 0;
    bool removable_ = false;
    bool solid_state_ = false;

    // Only one of the two interfaces is ever created: a drive is either reached over ATA or over
    // NVMe.
    IOCFPlugInInterface** plugin_ = nullptr;
    IOATASMARTInterface** ata_smart_ = nullptr;
    IONVMeSMARTInterface** nvme_smart_ = nullptr;

    // Enabling S.M.A.R.T. is only ever attempted once per drive.
    bool smart_enable_attempted_ = false;

    Q_DISABLE_COPY_MOVE(PhysicalDriveReaderMac)
};

#endif // BASE_PHYSICAL_DRIVE_READER_MAC_H
