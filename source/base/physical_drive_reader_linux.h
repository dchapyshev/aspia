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

#ifndef BASE_PHYSICAL_DRIVE_READER_LINUX_H
#define BASE_PHYSICAL_DRIVE_READER_LINUX_H

#include "base/physical_drive_reader.h"

class PhysicalDriveReaderLinux final : public PhysicalDriveReader
{
public:
    PhysicalDriveReaderLinux() = default;
    ~PhysicalDriveReaderLinux() final;

    // Paths of the physical drives present in the system.
    static QStringList devicePaths();

    // PhysicalDriveReader implementation.
    std::string model() const final { return model_; }
    std::string serialNumber() const final { return serial_number_; }
    std::string firmwareRevision() const final { return firmware_revision_; }
    BusType busType() const final { return bus_type_; }
    quint64 size() const final { return size_; }
    bool isRemovable() const final { return removable_; }
    MediaType mediaType() const final { return media_type_; }
    QByteArray ataIdentifyData() final;
    QByteArray ataSmartAttributes() final;
    QByteArray ataSmartThresholds() final;
    QByteArray nvmeHealthLog() final;

protected:
    // PhysicalDriveReader implementation.
    bool open(const QString& device_path) final;

private:
    // Contents of the sysfs attribute |name| of the drive. Empty when the driver does not have the
    // attribute or refused to answer.
    std::string sysAttribute(const QString& name) const;
    QByteArray sysAttributeData(const QString& name) const;

    void readIdentity();
    void readBusType();
    void readSize();

    // Issues an ATA command that returns a single sector, over the SCSI to ATA translation the
    // kernel and the controller provide. The arguments name the drive registers the command is
    // built from.
    QByteArray readAtaSector(quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command);

    // Sends a single ATA command. |cdb_size| selects the form of the pass-through command and
    // |sector| receives the data the command returns, or is nullptr for a command that transfers
    // nothing.
    bool ataPassThrough(quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command,
                        int cdb_size, char* sector);
    bool enableAtaSmart();

    int fd_ = -1;
    QString device_name_;
    BusType bus_type_ = BusType::UNKNOWN;
    std::string model_;
    std::string serial_number_;
    std::string firmware_revision_;
    quint64 size_ = 0;
    bool removable_ = false;
    MediaType media_type_ = MediaType::UNKNOWN;

    // Enabling S.M.A.R.T. is only ever attempted once per drive.
    bool smart_enable_attempted_ = false;

    Q_DISABLE_COPY_MOVE(PhysicalDriveReaderLinux)
};

#endif // BASE_PHYSICAL_DRIVE_READER_LINUX_H
