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

#ifndef BASE_WIN_PHYSICAL_DRIVE_READER_H
#define BASE_WIN_PHYSICAL_DRIVE_READER_H

#include <QByteArray>
#include <QStringList>

#include <qt_windows.h>
#include <winioctl.h>

#include "base/win/device.h"

// Reads the identification and the raw S.M.A.R.T. data of a single physical drive. The data blocks
// are returned exactly as the drive produced them and are meant to be handed to the platform
// independent parsers (AtaIdentify, AtaSmart, NvmeSmart).
class PhysicalDriveReader
{
public:
    PhysicalDriveReader() = default;
    ~PhysicalDriveReader() = default;

    // Paths of the physical drives present in the system.
    static QStringList devicePaths();

    // Opens the drive at |device_path| and reads the identification that the storage stack keeps for
    // every bus type. Returns false when the drive cannot be opened.
    bool open(const QString& device_path);

    QString model() const { return model_; }
    QString serialNumber() const { return serial_number_; }
    QString firmwareRevision() const { return firmware_revision_; }
    STORAGE_BUS_TYPE busType() const { return bus_type_; }
    quint64 size() const { return size_; }
    bool isRemovable() const { return removable_; }

    // True when the storage stack reports the drive as having no seek penalty. Unlike the rotation
    // rate of the ATA identification data, this is also known for NVMe drives.
    bool isSolidState() const { return solid_state_; }

    // Raw data blocks. Empty when the drive, or the bus it sits on, does not support the request.
    QByteArray ataIdentifyData();
    QByteArray ataSmartAttributes();
    QByteArray ataSmartThresholds();
    QByteArray nvmeHealthLog();

private:
    bool readDeviceDescriptor();
    bool readSeekPenalty();
    bool readSize();

    // Issues an ATA command that returns a single sector, over the transport the bus requires. The
    // arguments name the drive registers the command is built from.
    QByteArray readAtaSector(quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command);
    QByteArray readAtaSectorDirect(quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command);
    QByteArray readAtaSectorScsi(quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command);
    bool enableAtaSmart();

    Device device_;
    quint8 device_number_ = 0;
    STORAGE_BUS_TYPE bus_type_ = BusTypeUnknown;
    QString model_;
    QString serial_number_;
    QString firmware_revision_;
    quint64 size_ = 0;
    bool removable_ = false;
    bool solid_state_ = false;

    // Enabling S.M.A.R.T. is only ever attempted once per drive.
    bool smart_enable_attempted_ = false;

    Q_DISABLE_COPY_MOVE(PhysicalDriveReader)
};

#endif // BASE_WIN_PHYSICAL_DRIVE_READER_H
