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

#include "base/physical_drive_reader_linux.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <fcntl.h>
#include <linux/fs.h>
#include <linux/nvme_ioctl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>

#include "base/drive_smart.h"
#include "base/logging.h"

namespace {

constexpr qsizetype kSectorSize = 512;
constexpr int kSenseBufferSize = 32;

// Time the drive and every controller on the way to it are given to answer a command.
constexpr unsigned int kCommandTimeoutMs = 10 * 1000;

// A drive that stops answering keeps the enumeration busy, so the number of drives to look at is
// capped rather than trusting the device list to be short.
constexpr int kMaxDrives = 64;

// Operation codes of the ATA commands and of the S.M.A.R.T. subcommands issued below.
constexpr quint8 kAtaIdentifyDevice = 0xEC;
constexpr quint8 kAtaSmart = 0xB0;
constexpr quint8 kSmartReadAttributes = 0xD0;
constexpr quint8 kSmartReadThresholds = 0xD1;
constexpr quint8 kSmartEnableOperations = 0xD8;

// Cylinder register values that tell the drive the command belongs to the S.M.A.R.T. feature set.
constexpr quint8 kSmartCylLow = 0x4F;
constexpr quint8 kSmartCylHigh = 0xC2;

// Every ATA command this file issues transfers a single sector.
constexpr quint8 kSectorCount = 1;
constexpr quint8 kSectorNumber = 1;

// Drive/head register value selecting the device the handle was opened for.
constexpr quint8 kDriveHeadRegister = 0xA0;

// Operation codes of the two forms of the ATA PASS-THROUGH command and the protocols they carry.
constexpr quint8 kAtaPassThrough12 = 0xA1;
constexpr quint8 kAtaPassThrough16 = 0x85;
constexpr int kCdbSize12 = 12;
constexpr int kCdbSize16 = 16;
constexpr quint8 kAtaProtocolNonData = 3;
constexpr quint8 kAtaProtocolPioDataIn = 4;

// Operation code of the NVMe Get Log Page admin command and the namespace identifier that addresses
// the controller instead of a single namespace.
constexpr quint8 kNvmeGetLogPage = 0x02;
constexpr quint32 kNvmeNamespaceAll = 0xFFFFFFFF;

//--------------------------------------------------------------------------------------------------
// Directory the kernel keeps the attributes of the block device |name| in.
QString sysBlockPath(const QString& name)
{
    return QString("/sys/block/%1").arg(name);
}

//--------------------------------------------------------------------------------------------------
QString readSysFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    // Files under /sys report a size of 0, so QFile::size() cannot be used to reserve the buffer.
    return QString::fromLatin1(file.readAll()).trimmed();
}

//--------------------------------------------------------------------------------------------------
// Position of the first component of |path| that starts with |prefix|, or -1 when the path has no
// such component.
qsizetype componentIndex(const QStringList& path, const QString& prefix)
{
    for (qsizetype i = 0; i < path.count(); ++i)
    {
        if (path.at(i).startsWith(prefix))
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
// True when the libata port at |port_path| negotiated a serial ATA link speed. A parallel ATA port
// has no speed to report and the kernel answers with a placeholder for it.
bool isSataPort(const QString& port_path)
{
    const QStringList links = QDir(port_path).entryList(QStringList() << "link*", QDir::Dirs);

    for (const QString& link : links)
    {
        const QString path = QString("%1/%2/ata_link/%2/sata_spd").arg(port_path, link);
        const QString speed = readSysFile(path);

        if (!speed.isEmpty() && speed != "<unknown>")
            return true;
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
PhysicalDriveReaderLinux::~PhysicalDriveReaderLinux()
{
    if (fd_ != -1)
        close(fd_);
}

//--------------------------------------------------------------------------------------------------
// static
QStringList PhysicalDriveReaderLinux::devicePaths()
{
    QStringList result;

    const QStringList names = QDir("/sys/block").entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& name : names)
    {
        if (result.count() >= kMaxDrives)
            break;

        // A namespace of a multipath drive is listed once per controller it can be reached over and
        // once for the drive itself. The kernel hides the per controller entries and gives them no
        // device node, so only the entry that stands for the drive is left.
        if (readSysFile(sysBlockPath(name) + "/hidden") == "1")
            continue;

        // Loop, device mapper and software raid devices are listed as block devices, but have no
        // drive behind them. The kernel keeps all of them under /sys/devices/virtual. The multipath
        // entry of an NVMe drive lives there as well, because the kernel parents it to the class
        // device of the subsystem instead of to a controller, and behind that one there is a drive.
        const QString target = QFileInfo(sysBlockPath(name)).symLinkTarget();
        if (target.contains("/devices/virtual/") && !target.contains("/nvme-subsystem/"))
            continue;

        // Optical and floppy drives sit on a real bus, but have no health information to report.
        if (name.startsWith("sr") || name.startsWith("fd"))
            continue;

        result.append(QString("/dev/%1").arg(name));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderLinux::ataIdentifyData()
{
    // IDENTIFY DEVICE leaves the cylinder registers at zero.
    return readAtaSector(0, 0, 0, kAtaIdentifyDevice);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderLinux::ataSmartAttributes()
{
    QByteArray result = readAtaSector(kSmartReadAttributes, kSmartCylLow, kSmartCylHigh, kAtaSmart);
    if (!result.isEmpty())
        return result;

    // A drive with S.M.A.R.T. turned off rejects the read until the feature is enabled. Enabling it
    // changes a persistent setting of the drive, so it is only done after the read failed.
    if (!enableAtaSmart())
        return QByteArray();

    return readAtaSector(kSmartReadAttributes, kSmartCylLow, kSmartCylHigh, kAtaSmart);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderLinux::ataSmartThresholds()
{
    return readAtaSector(kSmartReadThresholds, kSmartCylLow, kSmartCylHigh, kAtaSmart);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderLinux::nvmeHealthLog()
{
    if (bus_type_ != BusType::NVME)
        return QByteArray();

    QByteArray buffer(NvmeSmart::kHealthLogSize, 0);

    // The length of the transfer is counted in dwords and is passed one less than the actual count.
    const quint32 dwords = static_cast<quint32>(NvmeSmart::kHealthLogSize / sizeof(quint32)) - 1;

    nvme_admin_cmd command;
    memset(&command, 0, sizeof(command));

    command.opcode = kNvmeGetLogPage;
    command.nsid = kNvmeNamespaceAll;
    command.addr = static_cast<quint64>(reinterpret_cast<quintptr>(buffer.data()));
    command.data_len = static_cast<quint32>(NvmeSmart::kHealthLogSize);
    command.cdw10 = NvmeSmart::kHealthLogPageId | (dwords << 16);
    command.timeout_ms = kCommandTimeoutMs;

    // A non-zero result is either the errno of a refused request or the status the controller
    // answered with.
    if (ioctl(fd_, NVME_IOCTL_ADMIN_CMD, &command) != 0)
    {
        PLOG(INFO) << "Drive" << device_name_ << "does not report NVMe health information";
        return QByteArray();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderLinux::open(const QString& device_path)
{
    // O_NONBLOCK keeps the call from waiting for a removable drive with no media in it. Read access
    // is enough for the requests below: passing a command through is allowed based on the privileges
    // of the process, not on the mode the drive was opened with.
    fd_ = ::open(device_path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ == -1)
    {
        PLOG(ERROR) << "Unable to open drive:" << device_path;
        return false;
    }

    device_name_ = QFileInfo(device_path).fileName();

    readIdentity();
    readBusType();
    readSize();

    return true;
}

//--------------------------------------------------------------------------------------------------
QString PhysicalDriveReaderLinux::sysAttribute(const QString& name) const
{
    return readSysFile(QString("%1/%2").arg(sysBlockPath(device_name_), name));
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderLinux::readIdentity()
{
    const QString vendor = sysAttribute("device/vendor");
    const QString model = sysAttribute("device/model");

    // The SCSI stack, which serves ATA drives as well, puts the name of the protocol into the vendor
    // of a drive that has none of its own, which says nothing about the model.
    if (vendor.isEmpty() || vendor == "ATA" || vendor == "NVMe")
        model_ = model;
    else if (model.isEmpty())
        model_ = vendor;
    else
        model_ = vendor + " " + model;

    serial_number_ = sysAttribute("device/serial");

    // The drivers name the same information differently: the SCSI stack has "rev", the NVMe stack
    // "firmware_rev".
    firmware_revision_ = sysAttribute("device/firmware_rev");
    if (firmware_revision_.isEmpty())
        firmware_revision_ = sysAttribute("device/rev");

    removable_ = sysAttribute("removable") == "1";
    solid_state_ = sysAttribute("queue/rotational") == "0";
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderLinux::readBusType()
{
    // The kernel does not name the bus, but every controller the requests pass through adds a
    // component to the device tree path of the drive.
    const QStringList path = QFileInfo(sysBlockPath(device_name_)).canonicalFilePath()
        .split('/', Qt::SkipEmptyParts);

    // The path is walked from the most specific transport to the least: a USB drive, for example, is
    // reached through the SCSI stack as well, so its path carries both components.
    if (componentIndex(path, "nvme") >= 0)
    {
        bus_type_ = BusType::NVME;
    }
    else if (componentIndex(path, "mmc_host") >= 0)
    {
        bus_type_ = sysAttribute("device/type") == "SD" ? BusType::SD : BusType::MMC;
    }
    else if (componentIndex(path, "usb") >= 0)
    {
        bus_type_ = BusType::USB;
    }
    else if (componentIndex(path, "fw-host") >= 0)
    {
        bus_type_ = BusType::IEEE1394;
    }
    else if (componentIndex(path, "session") >= 0)
    {
        bus_type_ = BusType::ISCSI;
    }
    else if (componentIndex(path, "end_device") >= 0)
    {
        bus_type_ = BusType::SAS;
    }
    else if (componentIndex(path, "virtio") >= 0)
    {
        bus_type_ = BusType::VIRTUAL;
    }
    else
    {
        const qsizetype ata_index = componentIndex(path, "ata");
        if (ata_index >= 0)
        {
            const QString port_path = "/" + QStringList(path.mid(0, ata_index + 1)).join('/');
            bus_type_ = isSataPort(port_path) ? BusType::SATA : BusType::ATA;
        }
        else if (componentIndex(path, "host") >= 0)
        {
            bus_type_ = BusType::SCSI;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderLinux::readSize()
{
    quint64 size = 0;

    if (ioctl(fd_, BLKGETSIZE64, &size) != 0)
    {
        PLOG(ERROR) << "Unable to query size of drive" << device_name_;
        return;
    }

    size_ = size;
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderLinux::readAtaSector(
    quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command)
{
    // NVMe drives do not speak ATA.
    if (bus_type_ == BusType::NVME)
        return QByteArray();

    QByteArray result(kSectorSize, 0);

    // The kernel and every controller worth asking answer the 16 byte form of the command. Only the
    // bridges of some USB enclosures are limited to the 12 byte one.
    if (!ataPassThrough(features, cyl_low, cyl_high, command, kCdbSize16, result.data()) &&
        !ataPassThrough(features, cyl_low, cyl_high, command, kCdbSize12, result.data()))
    {
        return QByteArray();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderLinux::ataPassThrough(quint8 features, quint8 cyl_low, quint8 cyl_high,
                                         quint8 command, int cdb_size, char* sector)
{
    quint8 cdb[kCdbSize16];
    memset(cdb, 0, sizeof(cdb));

    // A command that returns a sector reads it from the device (T_DIR), counts the transfer in
    // blocks (BYTE_BLOCK) and takes its length from the sector count register (T_LENGTH). All three
    // stay at zero for a command that transfers nothing.
    const quint8 protocol = sector ? kAtaProtocolPioDataIn : kAtaProtocolNonData;
    const quint8 transfer = sector ? ((1 << 3) | (1 << 2) | 2) : 0;

    if (cdb_size == kCdbSize16)
    {
        // The 16 byte form has room for the 16 bit registers of a 48-bit address command and holds
        // the low byte of every register in the second of its two bytes.
        cdb[0]  = kAtaPassThrough16;
        cdb[1]  = protocol << 1;
        cdb[2]  = transfer;
        cdb[4]  = features;
        cdb[6]  = kSectorCount;
        cdb[8]  = kSectorNumber;
        cdb[10] = cyl_low;
        cdb[12] = cyl_high;
        cdb[13] = kDriveHeadRegister;
        cdb[14] = command;
    }
    else
    {
        cdb[0] = kAtaPassThrough12;
        cdb[1] = protocol << 1;
        cdb[2] = transfer;
        cdb[3] = features;
        cdb[4] = kSectorCount;
        cdb[5] = kSectorNumber;
        cdb[6] = cyl_low;
        cdb[7] = cyl_high;
        cdb[8] = kDriveHeadRegister;
        cdb[9] = command;
    }

    quint8 sense[kSenseBufferSize];
    memset(sense, 0, sizeof(sense));

    sg_io_hdr_t request;
    memset(&request, 0, sizeof(request));

    request.interface_id    = 'S';
    request.cmd_len         = static_cast<unsigned char>(cdb_size);
    request.cmdp            = cdb;
    request.mx_sb_len       = sizeof(sense);
    request.sbp             = sense;
    request.dxfer_direction = sector ? SG_DXFER_FROM_DEV : SG_DXFER_NONE;
    request.dxfer_len       = sector ? static_cast<unsigned int>(kSectorSize) : 0;
    request.dxferp          = sector;
    request.timeout         = kCommandTimeoutMs;

    if (ioctl(fd_, SG_IO, &request) != 0)
    {
        // Passing an ATA command through is a privileged operation, so an unprivileged process is
        // refused before the request reaches the drive.
        PLOG(INFO) << "Unable to send ATA command" << command << "to drive" << device_name_;
        return false;
    }

    // A drive that rejected the command answers with a check condition. The sense data it comes
    // with describes the drive registers, which this class has no use for.
    if (request.status || request.host_status || request.driver_status)
        return false;

    // A translating controller that answered with less than a full sector left the rest of the
    // buffer untouched.
    return !sector || !request.resid;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderLinux::enableAtaSmart()
{
    if (smart_enable_attempted_)
        return false;

    smart_enable_attempted_ = true;

    return ataPassThrough(kSmartEnableOperations, kSmartCylLow, kSmartCylHigh, kAtaSmart,
                          kCdbSize16, nullptr) ||
           ataPassThrough(kSmartEnableOperations, kSmartCylLow, kSmartCylHigh, kAtaSmart,
                          kCdbSize12, nullptr);
}
