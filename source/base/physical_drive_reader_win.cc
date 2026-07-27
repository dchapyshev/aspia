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

#include "base/physical_drive_reader_win.h"

#include <SetupAPI.h>
#include <ntddscsi.h>

#include <cstddef>
#include <memory>

#include "base/drive_smart.h"
#include "base/logging.h"
#include "base/win/scoped_device_info.h"

namespace {

// GUID_DEVINTERFACE_DISK. Defined locally to keep the file free of a link time dependency on the
// system GUID library.
const GUID kDiskInterfaceGuid =
    { 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };

constexpr qsizetype kSectorSize = 512;
constexpr DWORD kSenseBufferSize = 32;

// Every ATA command this file issues transfers a single sector.
constexpr quint8 kSectorCount = 1;
constexpr quint8 kSectorNumber = 1;

// Drive/head register value selecting the device the handle was opened for.
constexpr quint8 kDriveHeadRegister = 0xA0;

// Operation code of the ATA PASS-THROUGH (12) command and the protocol it carries here.
constexpr quint8 kAtaPassThrough12 = 0xA1;
constexpr quint8 kAtaProtocolPioDataIn = 4;

// Time a translating controller is given to answer a pass-through command.
constexpr ULONG kScsiTimeoutSeconds = 10;

// A drive that stops answering keeps the enumeration busy, so the number of drives to look at is
// capped rather than trusting the device list to be short.
constexpr int kMaxDrives = 64;

//--------------------------------------------------------------------------------------------------
// Reads a string that |buffer| holds at |offset|. The offsets come from the storage driver, so they
// are validated against the buffer instead of being trusted.
QString stringFromDescriptor(const QByteArray& buffer, DWORD offset)
{
    if (!offset || offset >= static_cast<DWORD>(buffer.size()))
        return QString();

    const char* start = buffer.constData() + offset;
    const qsizetype length = qstrnlen(start, buffer.size() - offset);

    return QString::fromLatin1(start, length).trimmed();
}

//--------------------------------------------------------------------------------------------------
QString driveNumberToPath(DWORD device_number)
{
    return QString("\\\\.\\PhysicalDrive%1").arg(device_number);
}

//--------------------------------------------------------------------------------------------------
bool driveNumber(Device& device, DWORD* device_number)
{
    STORAGE_DEVICE_NUMBER number;
    memset(&number, 0, sizeof(number));

    DWORD bytes_returned = 0;

    if (!device.ioControl(IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number, sizeof(number),
                          &bytes_returned) || bytes_returned < sizeof(number))
    {
        return false;
    }

    *device_number = number.DeviceNumber;
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QStringList PhysicalDriveReaderWin::devicePaths()
{
    QStringList result;

    ScopedDeviceInfo device_info(SetupDiGetClassDevsW(&kDiskInterfaceGuid, nullptr, nullptr,
        DIGCF_PROFILE | DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!device_info.isValid())
    {
        PLOG(ERROR) << "SetupDiGetClassDevsW failed";
        return result;
    }

    SP_DEVICE_INTERFACE_DATA interface_data;
    memset(&interface_data, 0, sizeof(interface_data));
    interface_data.cbSize = sizeof(interface_data);

    for (DWORD index = 0; result.count() < kMaxDrives && SetupDiEnumDeviceInterfaces(
             device_info.get(), nullptr, &kDiskInterfaceGuid, index, &interface_data); ++index)
    {
        DWORD required_size = 0;
        if (SetupDiGetDeviceInterfaceDetailW(device_info.get(), &interface_data, nullptr, 0,
                                             &required_size, nullptr) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            continue;
        }

        std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(required_size);
        auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.get());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(device_info.get(), &interface_data, detail,
                                              required_size, &required_size, nullptr))
        {
            continue;
        }

        // The interface path is enough to ask the drive for its number, but the ATA ioctls address
        // the drive by number, so the number is what the caller is given.
        Device device;
        if (!device.open(QString::fromWCharArray(detail->DevicePath), 0,
                         FILE_SHARE_READ | FILE_SHARE_WRITE))
        {
            continue;
        }

        DWORD device_number = 0;
        if (!driveNumber(device, &device_number))
            continue;

        const QString path = driveNumberToPath(device_number);

        // A drive with more than one interface is listed once per interface.
        if (!result.contains(path))
            result.append(path);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// Maps the bus of the storage stack to the one the report is built from.
PhysicalDriveReader::BusType PhysicalDriveReaderWin::busType() const
{
    switch (bus_type_)
    {
        case BusTypeScsi:               return BusType::SCSI;
        case BusTypeAtapi:              return BusType::ATAPI;
        case BusTypeAta:                return BusType::ATA;
        case BusType1394:               return BusType::IEEE1394;
        case BusTypeSsa:                return BusType::SSA;
        case BusTypeFibre:              return BusType::FIBRE;
        case BusTypeUsb:                return BusType::USB;
        case BusTypeRAID:               return BusType::RAID;
        case BusTypeiScsi:              return BusType::ISCSI;
        case BusTypeSas:                return BusType::SAS;
        case BusTypeSata:               return BusType::SATA;
        case BusTypeSd:                 return BusType::SD;
        case BusTypeMmc:                return BusType::MMC;
        case BusTypeVirtual:            return BusType::VIRTUAL;
        case BusTypeFileBackedVirtual:  return BusType::FILE_BACKED_VIRTUAL;
        case BusTypeNvme:               return BusType::NVME;
        case BusTypeSpaces:             return BusType::SPACES;
        case BusTypeSCM:                return BusType::SCM;
        case BusTypeUfs:                return BusType::UFS;
        case BusTypeNvmeof:             return BusType::NVME_OF;
        default:                        return BusType::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::ataIdentifyData()
{
    // IDENTIFY DEVICE leaves the cylinder registers at zero.
    return readAtaSector(0, 0, 0, ID_CMD);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::ataSmartAttributes()
{
    QByteArray result = readAtaSector(READ_ATTRIBUTES, SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD);
    if (!result.isEmpty())
        return result;

    // A drive with S.M.A.R.T. turned off rejects the read until the feature is enabled. Enabling it
    // changes a persistent setting of the drive, so it is only done after the read failed.
    if (!enableAtaSmart())
        return QByteArray();

    return readAtaSector(READ_ATTRIBUTES, SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::ataSmartThresholds()
{
    return readAtaSector(READ_THRESHOLDS, SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::nvmeHealthLog()
{
    if (bus_type_ != BusTypeNvme)
        return QByteArray();

    const qsizetype protocol_data_offset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    const qsizetype buffer_size =
        sizeof(STORAGE_PROPERTY_QUERY) + protocol_data_offset + NvmeSmart::kHealthLogSize;

    QByteArray buffer(buffer_size, 0);

    auto* query = reinterpret_cast<STORAGE_PROPERTY_QUERY*>(buffer.data());
    query->PropertyId = StorageDeviceProtocolSpecificProperty;
    query->QueryType = PropertyStandardQuery;

    auto* protocol_data =
        reinterpret_cast<STORAGE_PROTOCOL_SPECIFIC_DATA*>(query->AdditionalParameters);
    protocol_data->ProtocolType = ProtocolTypeNvme;
    protocol_data->DataType = NVMeDataTypeLogPage;
    protocol_data->ProtocolDataRequestValue = NvmeSmart::kHealthLogPageId;
    protocol_data->ProtocolDataRequestSubValue = 0;
    protocol_data->ProtocolDataOffset = static_cast<DWORD>(protocol_data_offset);
    protocol_data->ProtocolDataLength = NvmeSmart::kHealthLogSize;

    DWORD bytes_returned = 0;

    if (!device_.ioControl(IOCTL_STORAGE_QUERY_PROPERTY, buffer.data(),
                           static_cast<DWORD>(buffer_size), buffer.data(),
                           static_cast<DWORD>(buffer_size), &bytes_returned))
    {
        LOG(INFO) << "Drive" << device_number_ << "does not report NVMe health information";
        return QByteArray();
    }

    if (bytes_returned < sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))
        return QByteArray();

    const auto* descriptor =
        reinterpret_cast<const STORAGE_PROTOCOL_DATA_DESCRIPTOR*>(buffer.constData());
    if (descriptor->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR) ||
        descriptor->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))
    {
        LOG(ERROR) << "Unexpected protocol data descriptor:" << descriptor->Version
                   << descriptor->Size;
        return QByteArray();
    }

    // The driver decides where inside the descriptor it puts the log, so the position it reports is
    // validated against the buffer that was handed to it and against the part of that buffer the
    // driver actually filled: everything past it is the zero fill of the request.
    const STORAGE_PROTOCOL_SPECIFIC_DATA& reply = descriptor->ProtocolSpecificData;
    const qsizetype log_offset =
        offsetof(STORAGE_PROTOCOL_DATA_DESCRIPTOR, ProtocolSpecificData) + reply.ProtocolDataOffset;
    const qsizetype log_end = log_offset + NvmeSmart::kHealthLogSize;

    if (reply.ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) ||
        reply.ProtocolDataLength < NvmeSmart::kHealthLogSize ||
        log_end > buffer.size() || log_end > static_cast<qsizetype>(bytes_returned))
    {
        LOG(ERROR) << "Protocol data outside of the reply buffer:" << reply.ProtocolDataOffset
                   << reply.ProtocolDataLength;
        return QByteArray();
    }

    return buffer.mid(log_offset, NvmeSmart::kHealthLogSize);
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderWin::open(const QString& device_path)
{
    // The ATA pass-through ioctls need write access while the property queries need no access at
    // all, so a drive that refuses to be opened for writing still reports its identification.
    if (!device_.open(device_path, GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE) &&
        !device_.open(device_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE) &&
        !device_.open(device_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE))
    {
        PLOG(ERROR) << "Unable to open drive:" << device_path;
        return false;
    }

    DWORD device_number = 0;
    if (!driveNumber(device_, &device_number))
    {
        PLOG(ERROR) << "Unable to get number of drive:" << device_path;
        return false;
    }

    device_number_ = static_cast<quint8>(device_number);

    readDeviceDescriptor();
    readSeekPenalty();
    readSize();

    return true;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderWin::readDeviceDescriptor()
{
    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));

    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    // The descriptor is followed by the strings it points at, so its length is asked for first.
    STORAGE_DESCRIPTOR_HEADER header;
    memset(&header, 0, sizeof(header));

    DWORD bytes_returned = 0;

    if (!device_.ioControl(IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header,
                           sizeof(header), &bytes_returned) ||
        header.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        PLOG(ERROR) << "Unable to query size of device descriptor";
        return false;
    }

    QByteArray buffer(header.Size, 0);

    if (!device_.ioControl(IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
                           static_cast<DWORD>(buffer.size()), &bytes_returned) ||
        bytes_returned < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
        PLOG(ERROR) << "Unable to query device descriptor";
        return false;
    }

    const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer.constData());

    bus_type_ = descriptor->BusType;
    removable_ = descriptor->RemovableMedia;

    const QString vendor = stringFromDescriptor(buffer, descriptor->VendorIdOffset);
    const QString product = stringFromDescriptor(buffer, descriptor->ProductIdOffset);

    if (vendor.isEmpty())
        model_ = product;
    else if (product.isEmpty())
        model_ = vendor;
    else
        model_ = vendor + " " + product;

    firmware_revision_ = stringFromDescriptor(buffer, descriptor->ProductRevisionOffset);
    serial_number_ = stringFromDescriptor(buffer, descriptor->SerialNumberOffset);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderWin::readSeekPenalty()
{
    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));

    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor;
    memset(&descriptor, 0, sizeof(descriptor));

    DWORD bytes_returned = 0;

    if (!device_.ioControl(IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor,
                           sizeof(descriptor), &bytes_returned) ||
        bytes_returned < sizeof(descriptor))
    {
        return false;
    }

    media_type_ = descriptor.IncursSeekPenalty ? MediaType::ROTATING : MediaType::SOLID_STATE;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderWin::readSize()
{
    GET_LENGTH_INFORMATION length;
    memset(&length, 0, sizeof(length));

    DWORD bytes_returned = 0;

    if (!device_.ioControl(IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
                           &bytes_returned) || bytes_returned < sizeof(length))
    {
        PLOG(ERROR) << "Unable to query size of drive" << device_number_;
        return false;
    }

    size_ = static_cast<quint64>(length.Length.QuadPart);
    return true;
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::readAtaSector(
    quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command)
{
    // NVMe drives do not speak ATA.
    if (bus_type_ == BusTypeNvme)
        return QByteArray();

    // The SMART ioctls only reach a drive that sits on an ATA controller. Everything else has to be
    // asked through a SCSI pass-through, which many controllers and USB bridges translate to ATA.
    if (bus_type_ == BusTypeAta || bus_type_ == BusTypeAtapi || bus_type_ == BusTypeSata)
    {
        QByteArray result = readAtaSectorDirect(features, cyl_low, cyl_high, command);
        if (!result.isEmpty())
            return result;
    }

    return readAtaSectorScsi(features, cyl_low, cyl_high, command);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::readAtaSectorDirect(
    quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command)
{
    SENDCMDINPARAMS request;
    memset(&request, 0, sizeof(request));

    request.cBufferSize                  = kSectorSize;
    request.bDriveNumber                 = device_number_;
    request.irDriveRegs.bFeaturesReg     = features;
    request.irDriveRegs.bSectorCountReg  = kSectorCount;
    request.irDriveRegs.bSectorNumberReg = kSectorNumber;
    request.irDriveRegs.bCylLowReg       = cyl_low;
    request.irDriveRegs.bCylHighReg      = cyl_high;
    request.irDriveRegs.bDriveHeadReg    = kDriveHeadRegister;
    request.irDriveRegs.bCommandReg      = command;

    // The structure ends with a single byte placeholder for the data the drive returns.
    QByteArray reply(sizeof(SENDCMDOUTPARAMS) + kSectorSize - 1, 0);
    DWORD bytes_returned = 0;

    if (!device_.ioControl(SMART_RCV_DRIVE_DATA, &request, sizeof(request), reply.data(),
                           static_cast<DWORD>(reply.size()), &bytes_returned))
    {
        return QByteArray();
    }

    if (bytes_returned < offsetof(SENDCMDOUTPARAMS, bBuffer) + kSectorSize)
        return QByteArray();

    const auto* result = reinterpret_cast<const SENDCMDOUTPARAMS*>(reply.constData());
    if (result->DriverStatus.bDriverError != SMART_NO_ERROR)
        return QByteArray();

    return QByteArray(reinterpret_cast<const char*>(result->bBuffer), kSectorSize);
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderWin::readAtaSectorScsi(
    quint8 features, quint8 cyl_low, quint8 cyl_high, quint8 command)
{
    struct Request
    {
        SCSI_PASS_THROUGH header;
        ULONG filler;
        UCHAR sense[kSenseBufferSize];
        UCHAR data[kSectorSize];
    };

    Request request;
    memset(&request, 0, sizeof(request));

    request.header.Length             = sizeof(request.header);
    request.header.CdbLength          = 12;
    request.header.SenseInfoLength    = kSenseBufferSize;
    request.header.SenseInfoOffset    = offsetof(Request, sense);
    request.header.DataIn             = SCSI_IOCTL_DATA_IN;
    request.header.DataTransferLength = kSectorSize;
    request.header.DataBufferOffset   = offsetof(Request, data);
    request.header.TimeOutValue       = kScsiTimeoutSeconds;

    request.header.Cdb[0] = kAtaPassThrough12;
    request.header.Cdb[1] = kAtaProtocolPioDataIn << 1;

    // T_DIR reads from the device, BYTE_BLOCK counts blocks and T_LENGTH takes the length from the
    // sector count register.
    request.header.Cdb[2] = (1 << 3) | (1 << 2) | 2;
    request.header.Cdb[3] = features;
    request.header.Cdb[4] = kSectorCount;
    request.header.Cdb[5] = kSectorNumber;
    request.header.Cdb[6] = cyl_low;
    request.header.Cdb[7] = cyl_high;
    request.header.Cdb[8] = kDriveHeadRegister;
    request.header.Cdb[9] = command;

    DWORD bytes_returned = 0;

    if (!device_.ioControl(IOCTL_SCSI_PASS_THROUGH, &request, sizeof(request.header), &request,
                           sizeof(request), &bytes_returned))
    {
        return QByteArray();
    }

    if (bytes_returned < offsetof(Request, data) + kSectorSize || request.header.ScsiStatus)
        return QByteArray();

    return QByteArray(reinterpret_cast<const char*>(request.data), kSectorSize);
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderWin::enableAtaSmart()
{
    if (smart_enable_attempted_)
        return false;

    smart_enable_attempted_ = true;

    // Only a drive on an ATA controller is asked to turn the feature on. A translating controller
    // that refused to read the attributes is unlikely to pass a state changing command either.
    if (bus_type_ != BusTypeAta && bus_type_ != BusTypeAtapi && bus_type_ != BusTypeSata)
        return false;

    SENDCMDINPARAMS request;
    memset(&request, 0, sizeof(request));

    request.cBufferSize                  = 0;
    request.bDriveNumber                 = device_number_;
    request.irDriveRegs.bFeaturesReg     = ENABLE_SMART;
    request.irDriveRegs.bSectorCountReg  = kSectorCount;
    request.irDriveRegs.bSectorNumberReg = kSectorNumber;
    request.irDriveRegs.bCylLowReg       = SMART_CYL_LOW;
    request.irDriveRegs.bCylHighReg      = SMART_CYL_HI;
    request.irDriveRegs.bDriveHeadReg    = kDriveHeadRegister;
    request.irDriveRegs.bCommandReg      = SMART_CMD;

    SENDCMDOUTPARAMS reply;
    memset(&reply, 0, sizeof(reply));

    DWORD bytes_returned = 0;

    if (!device_.ioControl(SMART_SEND_DRIVE_COMMAND, &request, sizeof(request), &reply,
                           sizeof(reply), &bytes_returned))
    {
        LOG(INFO) << "Unable to enable S.M.A.R.T. on drive" << device_number_;
        return false;
    }

    return reply.DriverStatus.bDriverError == SMART_NO_ERROR;
}
