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

#include "base/physical_drive_reader_mac.h"

#include <QFileInfo>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>

#include <cstring>

#include "base/drive_smart.h"
#include "base/logging.h"

// The blocks the drive returns are handed to the parsers as they are, so their layout has to be the
// one the parsers expect.
static_assert(sizeof(ATASMARTData) == 512, "Unexpected size of the S.M.A.R.T. data sector");
static_assert(sizeof(ATASMARTDataThresholds) == 512, "Unexpected size of the threshold sector");
static_assert(sizeof(NVMeSMARTData) == NvmeSmart::kHealthLogSize,
              "Unexpected size of the NVMe health log");

namespace {

// A drive that stops answering keeps the enumeration busy, so the number of drives to look at is
// capped rather than trusting the device list to be short.
constexpr int kMaxDrives = 64;

//--------------------------------------------------------------------------------------------------
QString stringProperty(io_registry_entry_t entry, CFStringRef key)
{
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    if (!value)
        return QString();

    QString result;
    if (CFGetTypeID(value) == CFStringGetTypeID())
        result = QString::fromCFString(static_cast<CFStringRef>(value)).trimmed();

    CFRelease(value);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool boolProperty(io_registry_entry_t entry, CFStringRef key)
{
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    if (!value)
        return false;

    bool result = false;
    if (CFGetTypeID(value) == CFBooleanGetTypeID())
        result = CFBooleanGetValue(static_cast<CFBooleanRef>(value));

    CFRelease(value);
    return result;
}

//--------------------------------------------------------------------------------------------------
quint64 numberProperty(io_registry_entry_t entry, CFStringRef key)
{
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    if (!value)
        return 0;

    long long result = 0;
    if (CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberLongLongType, &result);

    CFRelease(value);
    return static_cast<quint64>(result);
}

//--------------------------------------------------------------------------------------------------
// Value of |key| inside the dictionary the driver publishes under |dictionary_key|. The driver
// groups what it knows about the drive into such dictionaries.
QString characteristic(io_registry_entry_t entry, CFStringRef dictionary_key, CFStringRef key)
{
    CFTypeRef dictionary =
        IORegistryEntryCreateCFProperty(entry, dictionary_key, kCFAllocatorDefault, 0);
    if (!dictionary)
        return QString();

    QString result;

    if (CFGetTypeID(dictionary) == CFDictionaryGetTypeID())
    {
        CFTypeRef value = CFDictionaryGetValue(static_cast<CFDictionaryRef>(dictionary), key);
        if (value && CFGetTypeID(value) == CFStringGetTypeID())
            result = QString::fromCFString(static_cast<CFStringRef>(value)).trimmed();
    }

    CFRelease(dictionary);
    return result;
}

//--------------------------------------------------------------------------------------------------
// The device the media at |media| belongs to. Only the media published by a block storage driver
// has one: the containers synthesized by APFS or by a software raid are media as well, but they sit
// on top of a partition instead of a drive.
io_service_t blockStorageDevice(io_service_t media)
{
    io_service_t driver = IO_OBJECT_NULL;
    if (IORegistryEntryGetParentEntry(media, kIOServicePlane, &driver) != KERN_SUCCESS)
        return IO_OBJECT_NULL;

    io_service_t device = IO_OBJECT_NULL;

    if (IOObjectConformsTo(driver, kIOBlockStorageDriverClass))
    {
        if (IORegistryEntryGetParentEntry(driver, kIOServicePlane, &device) != KERN_SUCCESS)
            device = IO_OBJECT_NULL;
    }

    IOObjectRelease(driver);

    if (device && !IOObjectConformsTo(device, kIOBlockStorageDeviceClass))
    {
        IOObjectRelease(device);
        device = IO_OBJECT_NULL;
    }

    return device;
}

//--------------------------------------------------------------------------------------------------
PhysicalDriveReader::BusType busTypeFromInterconnect(const QString& interconnect)
{
    using BusType = PhysicalDriveReader::BusType;

    if (interconnect == kIOPropertyPhysicalInterconnectTypeATA)
        return BusType::ATA;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeSerialATA)
        return BusType::SATA;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeSerialAttachedSCSI)
        return BusType::SAS;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeATAPI)
        return BusType::ATAPI;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeUSB)
        return BusType::USB;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeFireWire)
        return BusType::IEEE1394;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeSecureDigital)
        return BusType::SD;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeSCSIParallel)
        return BusType::SCSI;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeFibreChannel)
        return BusType::FIBRE;
    if (interconnect == kIOPropertyPhysicalInterconnectTypeVirtual)
        return BusType::VIRTUAL;

    // A drive on the PCI bus speaks NVMe. Apple Fabric is what the storage of an Apple silicon
    // machine sits on and it speaks NVMe too.
    if (interconnect == kIOPropertyPhysicalInterconnectTypePCI ||
        interconnect == kIOPropertyPhysicalInterconnectTypePCIExpress ||
        interconnect == kIOPropertyPhysicalInterconnectTypeAppleFabric)
    {
        return BusType::NVME;
    }

    return BusType::UNKNOWN;
}

} // namespace

//--------------------------------------------------------------------------------------------------
PhysicalDriveReaderMac::~PhysicalDriveReaderMac()
{
    if (ata_smart_)
        (*ata_smart_)->Release(ata_smart_);

    if (nvme_smart_)
        (*nvme_smart_)->Release(nvme_smart_);

    if (plugin_)
        IODestroyPlugInInterface(plugin_);
}

//--------------------------------------------------------------------------------------------------
// static
QStringList PhysicalDriveReaderMac::devicePaths()
{
    QStringList result;

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t status = IOServiceGetMatchingServices(
        kIOMainPortDefault, IOServiceMatching(kIOMediaClass), &iterator);
    if (status != KERN_SUCCESS)
    {
        LOG(ERROR) << "IOServiceGetMatchingServices failed:" << status;
        return result;
    }

    io_service_t media = IO_OBJECT_NULL;

    while (result.count() < kMaxDrives && (media = IOIteratorNext(iterator)))
    {
        // Every partition of a drive is a media as well, only the one that covers the whole drive
        // is of interest here.
        if (boolProperty(media, CFSTR(kIOMediaWholeKey)))
        {
            io_service_t device = blockStorageDevice(media);
            if (device)
            {
                const QString interconnect =
                    characteristic(device, CFSTR(kIOPropertyProtocolCharacteristicsKey),
                                   CFSTR(kIOPropertyPhysicalInterconnectTypeKey));

                // A mounted disk image is a block storage device with no drive behind it.
                if (busTypeFromInterconnect(interconnect) != BusType::VIRTUAL)
                {
                    const QString bsd_name = stringProperty(media, CFSTR(kIOBSDNameKey));
                    if (!bsd_name.isEmpty())
                        result.append(QString("/dev/%1").arg(bsd_name));
                }

                IOObjectRelease(device);
            }
        }

        IOObjectRelease(media);
    }

    IOObjectRelease(iterator);
    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderMac::ataIdentifyData()
{
    if (!ata_smart_)
        return QByteArray();

    QByteArray result(AtaIdentify::kDataSize, 0);
    UInt32 bytes_read = 0;

    // The words are returned in the byte order of the host, which on every machine macOS runs on is
    // the order the parsers expect.
    IOReturn status = (*ata_smart_)->GetATAIdentifyData(
        ata_smart_, result.data(), static_cast<UInt32>(result.size()), &bytes_read);
    if (status != kIOReturnSuccess || bytes_read < result.size())
    {
        LOG(INFO) << "Unable to read identification of drive:" << status;
        return QByteArray();
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderMac::ataSmartAttributes()
{
    if (!ata_smart_)
        return QByteArray();

    ATASMARTData data;
    memset(&data, 0, sizeof(data));

    IOReturn status = (*ata_smart_)->SMARTReadData(ata_smart_, &data);
    if (status != kIOReturnSuccess)
    {
        // A drive with S.M.A.R.T. turned off rejects the read until the feature is enabled.
        // Enabling it changes a persistent setting of the drive, so it is only done after the read
        // failed.
        if (!enableAtaSmart())
            return QByteArray();

        status = (*ata_smart_)->SMARTReadData(ata_smart_, &data);
        if (status != kIOReturnSuccess)
            return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char*>(&data), sizeof(data));
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderMac::ataSmartThresholds()
{
    if (!ata_smart_)
        return QByteArray();

    ATASMARTDataThresholds thresholds;
    memset(&thresholds, 0, sizeof(thresholds));

    if ((*ata_smart_)->SMARTReadDataThresholds(ata_smart_, &thresholds) != kIOReturnSuccess)
        return QByteArray();

    return QByteArray(reinterpret_cast<const char*>(&thresholds), sizeof(thresholds));
}

//--------------------------------------------------------------------------------------------------
QByteArray PhysicalDriveReaderMac::nvmeHealthLog()
{
    if (!nvme_smart_)
        return QByteArray();

    NVMeSMARTData data;
    memset(&data, 0, sizeof(data));

    IOReturn status = (*nvme_smart_)->SMARTReadData(nvme_smart_, &data);
    if (status != kIOReturnSuccess)
    {
        LOG(INFO) << "Drive does not report NVMe health information:" << status;
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char*>(&data), sizeof(data));
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderMac::open(const QString& device_path)
{
    // The registry addresses a drive by its BSD name, which is the file name of its device node.
    const QString bsd_name = QFileInfo(device_path).fileName();

    io_service_t media = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOBSDNameMatching(kIOMainPortDefault, 0, bsd_name.toLocal8Bit().constData()));
    if (!media)
    {
        LOG(ERROR) << "Unable to find drive:" << device_path;
        return false;
    }

    readMedia(media);

    io_service_t device = blockStorageDevice(media);
    IOObjectRelease(media);

    if (!device)
    {
        LOG(ERROR) << "Drive has no block storage device:" << device_path;
        return false;
    }

    readIdentity(device);
    openSmartInterface(device);

    IOObjectRelease(device);
    return true;
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderMac::readMedia(io_service_t media)
{
    size_ = numberProperty(media, CFSTR(kIOMediaSizeKey));
    removable_ = boolProperty(media, CFSTR(kIOMediaRemovableKey));
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderMac::readIdentity(io_service_t device)
{
    const QString vendor = characteristic(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                          CFSTR(kIOPropertyVendorNameKey));
    const QString product = characteristic(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                           CFSTR(kIOPropertyProductNameKey));

    if (vendor.isEmpty())
        model_ = product;
    else if (product.isEmpty())
        model_ = vendor;
    else
        model_ = vendor + " " + product;

    serial_number_ = characteristic(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                    CFSTR(kIOPropertyProductSerialNumberKey));
    firmware_revision_ = characteristic(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                        CFSTR(kIOPropertyProductRevisionLevelKey));

    const QString medium = characteristic(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                          CFSTR(kIOPropertyMediumTypeKey));
    solid_state_ = medium == kIOPropertyMediumTypeSolidStateKey;

    bus_type_ = busTypeFromInterconnect(
        characteristic(device, CFSTR(kIOPropertyProtocolCharacteristicsKey),
                       CFSTR(kIOPropertyPhysicalInterconnectTypeKey)));
}

//--------------------------------------------------------------------------------------------------
void PhysicalDriveReaderMac::openSmartInterface(io_service_t device)
{
    // The driver announces which of the two user clients the drive answers, and it announces
    // neither for a drive that reports no health information at all.
    const bool nvme = boolProperty(device, CFSTR(kIOPropertyNVMeSMARTCapableKey));

    if (!nvme && !boolProperty(device, CFSTR(kIOPropertySMARTCapableKey)))
        return;

    SInt32 score = 0;
    kern_return_t status = IOCreatePlugInInterfaceForService(
        device, nvme ? kIONVMeSMARTUserClientTypeID : kIOATASMARTUserClientTypeID,
        kIOCFPlugInInterfaceID, &plugin_, &score);
    if (status != KERN_SUCCESS || !plugin_)
    {
        LOG(INFO) << "Drive provides no S.M.A.R.T. user client:" << status;
        plugin_ = nullptr;
        return;
    }

    HRESULT result = (*plugin_)->QueryInterface(
        plugin_, CFUUIDGetUUIDBytes(nvme ? kIONVMeSMARTInterfaceID : kIOATASMARTInterfaceID),
        nvme ? reinterpret_cast<LPVOID*>(&nvme_smart_) : reinterpret_cast<LPVOID*>(&ata_smart_));
    if (result != S_OK)
    {
        LOG(ERROR) << "Unable to query S.M.A.R.T. interface:" << result;

        IODestroyPlugInInterface(plugin_);
        plugin_ = nullptr;
        ata_smart_ = nullptr;
        nvme_smart_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
bool PhysicalDriveReaderMac::enableAtaSmart()
{
    if (smart_enable_attempted_)
        return false;

    smart_enable_attempted_ = true;

    return (*ata_smart_)->SMARTEnableDisableOperations(ata_smart_, true) == kIOReturnSuccess;
}
