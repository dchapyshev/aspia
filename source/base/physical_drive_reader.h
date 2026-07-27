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

#ifndef BASE_PHYSICAL_DRIVE_READER_H
#define BASE_PHYSICAL_DRIVE_READER_H

#include <QByteArray>
#include <QStringList>

#include <memory>

// Reads the identification and the raw S.M.A.R.T. data of a single physical drive. The data blocks
// are returned exactly as the drive produced them and are meant to be handed to the platform
// independent parsers (AtaIdentify, AtaSmart, NvmeSmart).
class PhysicalDriveReader
{
public:
    virtual ~PhysicalDriveReader() = default;

    // Transport the drive is reached over.
    enum class BusType
    {
        UNKNOWN             = 0,
        SCSI                = 1,
        ATAPI               = 2,
        ATA                 = 3,
        IEEE1394            = 4,
        SSA                 = 5,
        FIBRE               = 6,
        USB                 = 7,
        RAID                = 8,
        ISCSI               = 9,
        SAS                 = 10,
        SATA                = 11,
        SD                  = 12,
        MMC                 = 13,
        VIRTUAL             = 14,
        FILE_BACKED_VIRTUAL = 15,
        NVME                = 16,
        SPACES              = 17,
        SCM                 = 18,
        UFS                 = 19,
        NVME_OF             = 20
    };

    // What the drive stores the data on.
    enum class MediaType
    {
        UNKNOWN     = 0,
        ROTATING    = 1,
        SOLID_STATE = 2
    };

    // Paths of the physical drives present in the system. The shape of a path is specific to the
    // operating system.
    static QStringList devicePaths();

    // Creates a reader for the drive at |device_path|. Returns nullptr when the drive cannot be
    // opened or the operating system is not supported.
    static std::unique_ptr<PhysicalDriveReader> create(const QString& device_path);

    virtual QString model() const = 0;
    virtual QString serialNumber() const = 0;
    virtual QString firmwareRevision() const = 0;
    virtual BusType busType() const = 0;
    virtual quint64 size() const = 0;
    virtual bool isRemovable() const = 0;

    // Media the operating system reports the drive as having. Unlike the rotation rate of the ATA
    // identification data, this is also known for NVMe drives.
    virtual MediaType mediaType() const = 0;

    // Raw data blocks. Empty when the drive, or the bus it sits on, does not support the request.
    virtual QByteArray ataIdentifyData() = 0;
    virtual QByteArray ataSmartAttributes() = 0;
    virtual QByteArray ataSmartThresholds() = 0;
    virtual QByteArray nvmeHealthLog() = 0;

protected:
    PhysicalDriveReader() = default;

    // Opens the drive at |device_path| and reads the identification the operating system keeps for
    // every bus type. Called by create() and by nothing else.
    virtual bool open(const QString& device_path) = 0;

private:
    Q_DISABLE_COPY_MOVE(PhysicalDriveReader)
};

#endif // BASE_PHYSICAL_DRIVE_READER_H
