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

#ifndef BASE_SMBIOS_PARSER_H
#define BASE_SMBIOS_PARSER_H

#include <QByteArray>

#include <string>
#include <vector>

#include "base/smbios.h"

class SmbiosTableEnumerator
{
public:
    explicit SmbiosTableEnumerator(const QByteArray& smbios_data);
    ~SmbiosTableEnumerator();

    const SmbiosTable* table() const;

    bool isAtEnd() const;
    void advance();

    quint8 majorVersion() const;
    quint8 minorVersion() const;
    quint32 length() const;

private:
    void validate();

    SmbiosDump smbios_;

    quint8* start_ = nullptr;
    quint8* end_ = nullptr;
    quint8* pos_ = nullptr;
    quint8* next_ = nullptr;

    Q_DISABLE_COPY_MOVE(SmbiosTableEnumerator)
};

std::string smbiosString(const SmbiosTable* table, quint8 number);

class SmbiosBios
{
public:
    explicit SmbiosBios(const SmbiosTable* table);

    bool isValid() const;
    std::string vendor() const;
    std::string version() const;
    std::string releaseDate() const;
    quint32 address() const;
    quint64 romSize() const;
    std::string revision() const;
    std::string firmwareRevision() const;
    std::vector<std::string> characteristics() const;

private:
    const SmbiosBiosTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosBios)
};

class SmbiosSystem
{
public:
    explicit SmbiosSystem(const SmbiosTable* table);

    bool isValid() const;
    QByteArray uuid() const;

private:
    const SmbiosSystemTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosSystem)
};

class SmbiosBaseboard
{
public:
    explicit SmbiosBaseboard(const SmbiosTable* table);

    bool isValid() const;
    std::string manufacturer() const;
    std::string product() const;
    std::string version() const;
    std::string serialNumber() const;
    std::string assetTag() const;
    std::string location() const;
    std::string type() const;
    bool isHostingBoard() const;
    bool requiresDaughterBoard() const;
    bool isRemovable() const;
    bool isReplaceable() const;
    bool isHotSwappable() const;

private:
    quint8 featureFlags() const;

    const SmbiosBaseboardTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosBaseboard)
};

class SmbiosChassis
{
public:
    explicit SmbiosChassis(const SmbiosTable* table);

    bool isValid() const;
    std::string manufacturer() const;
    std::string version() const;
    std::string serialNumber() const;
    std::string assetTag() const;
    std::string skuNumber() const;
    std::string type() const;
    bool isLockPresent() const;
    std::string bootUpState() const;
    std::string powerSupplyState() const;
    std::string thermalState() const;
    std::string securityStatus() const;
    quint32 height() const;
    quint32 powerCords() const;

private:
    const SmbiosChassisTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosChassis)
};

class SmbiosProcessor
{
public:
    explicit SmbiosProcessor(const SmbiosTable* table);

    bool isValid() const;
    bool isPopulated() const;
    std::string manufacturer() const;
    std::string version() const;
    std::string serialNumber() const;
    std::string assetTag() const;
    std::string partNumber() const;
    std::string socketDesignation() const;
    std::string type() const;
    std::string family() const;
    std::string status() const;
    std::string upgrade() const;
    std::string socketType() const;
    quint64 id() const;
    double voltage() const;
    quint32 externalClock() const;
    quint32 maxSpeed() const;
    quint32 currentSpeed() const;
    quint32 coreCount() const;
    quint32 coreEnabled() const;
    quint32 threadCount() const;
    quint32 threadEnabled() const;
    quint16 l1CacheHandle() const;
    quint16 l2CacheHandle() const;
    quint16 l3CacheHandle() const;
    bool is64Bit() const;
    bool isMultiCore() const;
    bool isHardwareThread() const;
    bool isExecuteProtection() const;
    bool isEnhancedVirtualization() const;
    bool isPowerPerformanceControl() const;

private:
    quint16 characteristics() const;

    const SmbiosProcessorTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosProcessor)
};

class SmbiosCache
{
public:
    explicit SmbiosCache(const SmbiosTable* table);

    bool isValid() const;
    bool isEnabled() const;
    bool isSocketed() const;
    std::string designation() const;
    std::string location() const;
    std::string mode() const;
    std::string type() const;
    std::string errorCorrectionType() const;
    std::string associativity() const;
    std::string currentSramType() const;
    quint8 level() const;
    quint64 maxSize() const;
    quint64 currentSize() const;
    quint32 speed() const;
    bool supportsNonBurst() const;
    bool supportsBurst() const;
    bool supportsPipelineBurst() const;
    bool supportsSynchronous() const;
    bool supportsAsynchronous() const;

private:
    const SmbiosCacheTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosCache)
};

class SmbiosPortConnector
{
public:
    explicit SmbiosPortConnector(const SmbiosTable* table);

    bool isValid() const;
    std::string internalDesignator() const;
    std::string internalConnectorType() const;
    std::string externalDesignator() const;
    std::string externalConnectorType() const;
    std::string type() const;

private:
    const SmbiosPortConnectorTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPortConnector)
};

class SmbiosSystemSlot
{
public:
    explicit SmbiosSystemSlot(const SmbiosTable* table);

    bool isValid() const;
    std::string designation() const;
    std::string type() const;
    std::string dataBusWidth() const;
    std::string usage() const;
    std::string length() const;
    quint16 id() const;
    bool hasBusAddress() const;
    quint16 segmentGroupNumber() const;
    quint8 busNumber() const;
    quint8 deviceNumber() const;
    quint8 functionNumber() const;
    bool provides5Volts() const;
    bool provides3Volts() const;
    bool isShared() const;
    bool supportsPme() const;
    bool supportsHotPlug() const;
    bool supportsSmbus() const;
    bool supportsBifurcation() const;

private:
    quint8 characteristics1() const;
    quint8 characteristics2() const;

    const SmbiosSystemSlotTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosSystemSlot)
};

class SmbiosOnBoardDevices
{
public:
    explicit SmbiosOnBoardDevices(const SmbiosTable* table);

    bool isValid() const;
    int count() const;
    std::string description(int index) const;
    std::string type(int index) const;
    bool isEnabled(int index) const;

private:
    const quint8* device(int index) const;

    const SmbiosOnBoardDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosOnBoardDevices)
};

// Reads the OEM strings (Type 11) as well as the system configuration options (Type 12): the two
// tables differ only in the meaning of the strings they hold.
class SmbiosStringList
{
public:
    explicit SmbiosStringList(const SmbiosTable* table);

    bool isValid() const;
    int count() const;
    std::string string(int index) const;

private:
    const SmbiosStringListTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosStringList)
};

class SmbiosMemoryArray
{
public:
    explicit SmbiosMemoryArray(const SmbiosTable* table);

    bool isValid() const;
    std::string location() const;
    std::string use() const;
    std::string errorCorrection() const;
    quint64 maxCapacity() const;
    quint16 deviceCount() const;

private:
    const SmbiosMemoryArrayTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosMemoryArray)
};

class SmbiosMemoryDevice
{
public:
    explicit SmbiosMemoryDevice(const SmbiosTable* table);

    bool isValid() const;
    bool isPresent() const;
    std::string location() const;
    std::string bankLocator() const;
    std::string manufacturer() const;
    std::string serialNumber() const;
    std::string assetTag() const;
    quint64 size() const;
    std::string type() const;
    std::string formFactor() const;
    std::string technology() const;
    std::string partNumber() const;
    std::string firmwareVersion() const;

    // The speed and the configured speed of the device, in MT/s. Zero when unknown.
    quint32 speed() const;
    quint32 configuredSpeed() const;

    quint16 totalWidth() const;
    quint16 dataWidth() const;
    quint8 rank() const;
    quint32 minVoltage() const;
    quint32 maxVoltage() const;
    quint32 configuredVoltage() const;
    quint64 nonVolatileSize() const;
    quint64 volatileSize() const;
    quint64 cacheSize() const;
    quint64 logicalSize() const;
    std::vector<std::string> typeDetail() const;
    quint16 arrayHandle() const;

private:
    const SmbiosMemoryDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosMemoryDevice)
};

class SmbiosMemoryError
{
public:
    explicit SmbiosMemoryError(const SmbiosTable* table);

    bool isValid() const;
    std::string type() const;
    std::string granularity() const;
    std::string operation() const;
    quint32 vendorSyndrome() const;
    quint64 arrayErrorAddress() const;
    quint64 deviceErrorAddress() const;
    quint64 errorResolution() const;

private:
    const SmbiosMemoryErrorTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosMemoryError)
};

class SmbiosMemoryArrayAddress
{
public:
    explicit SmbiosMemoryArrayAddress(const SmbiosTable* table);

    bool isValid() const;
    quint64 startAddress() const;
    quint64 endAddress() const;
    quint64 size() const;
    quint16 arrayHandle() const;
    quint8 partitionWidth() const;

private:
    bool isExtended() const;

    const SmbiosMemoryArrayAddressTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosMemoryArrayAddress)
};

class SmbiosMemoryDeviceAddress
{
public:
    explicit SmbiosMemoryDeviceAddress(const SmbiosTable* table);

    bool isValid() const;
    quint64 startAddress() const;
    quint64 endAddress() const;
    quint64 size() const;
    quint16 deviceHandle() const;
    quint16 arrayAddressHandle() const;
    quint8 rowPosition() const;
    quint8 interleavePosition() const;
    quint8 interleaveDepth() const;

private:
    bool isExtended() const;

    const SmbiosMemoryDeviceAddressTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosMemoryDeviceAddress)
};

class SmbiosPointingDevice
{
public:
    explicit SmbiosPointingDevice(const SmbiosTable* table);

    bool isValid() const;
    std::string type() const;
    std::string interfaceType() const;
    quint8 buttonCount() const;

private:
    const SmbiosPointingDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPointingDevice)
};

class SmbiosPortableBattery
{
public:
    explicit SmbiosPortableBattery(const SmbiosTable* table);

    bool isValid() const;
    std::string location() const;
    std::string manufacturer() const;
    std::string manufactureDate() const;
    std::string serialNumber() const;
    std::string deviceName() const;
    std::string chemistry() const;
    std::string sbdsVersion() const;
    quint32 designCapacity() const;
    quint32 designVoltage() const;

    // The maximum error in the battery data, in percent. Negative when unknown.
    int maxError() const;

private:
    const SmbiosPortableBatteryTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPortableBattery)
};

// Reads the voltage probe (Type 26), the temperature probe (Type 28) and the electrical current
// probe (Type 29): the three tables share their layout, only the unit of the values differs.
class SmbiosProbe
{
public:
    explicit SmbiosProbe(const SmbiosTable* table);

    bool isValid() const;
    std::string description() const;
    std::string location() const;
    std::string status() const;

    // Millivolts for a voltage probe, milliamps for a current probe and tenths of a degree Celsius
    // for a temperature probe. Zero when the firmware does not report the value.
    qint32 maxValue() const;
    qint32 minValue() const;
    qint32 nominalValue() const;
    qint32 tolerance() const;

    // Tenths of the unit of the values above, and thousandths of a degree Celsius for a
    // temperature probe. Zero when unknown.
    qint32 resolution() const;

    // Hundredths of a percent. Zero when unknown.
    qint32 accuracy() const;

private:
    const SmbiosProbeTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosProbe)
};

class SmbiosCoolingDevice
{
public:
    explicit SmbiosCoolingDevice(const SmbiosTable* table);

    bool isValid() const;
    std::string description() const;
    std::string type() const;
    std::string status() const;

    // The group of the cooling unit the device belongs to. Zero when it belongs to none.
    quint8 unitGroup() const;

    // Revolutions per minute. Zero when the firmware does not report the speed.
    quint32 nominalSpeed() const;

private:
    const SmbiosCoolingDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosCoolingDevice)
};

class SmbiosSystemBoot
{
public:
    explicit SmbiosSystemBoot(const SmbiosTable* table);

    bool isValid() const;
    std::string status() const;

private:
    const SmbiosSystemBootTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosSystemBoot)
};

class SmbiosPowerSupply
{
public:
    explicit SmbiosPowerSupply(const SmbiosTable* table);

    bool isValid() const;
    bool isPresent() const;
    bool isUnplugged() const;
    bool isHotReplaceable() const;
    quint8 unitGroup() const;
    std::string location() const;
    std::string deviceName() const;
    std::string manufacturer() const;
    std::string serialNumber() const;
    std::string assetTag() const;
    std::string modelPartNumber() const;
    std::string revisionLevel() const;
    std::string type() const;
    std::string status() const;
    std::string inputVoltageRangeSwitching() const;

    // The maximum sustained power output, in milliwatts. Zero when unknown.
    quint32 maxPowerCapacity() const;

private:
    const SmbiosPowerSupplyTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPowerSupply)
};

class SmbiosAdditionalInfo
{
public:
    explicit SmbiosAdditionalInfo(const SmbiosTable* table);

    bool isValid() const;
    int count() const;
    quint16 referencedHandle(int index) const;
    quint8 referencedOffset(int index) const;
    std::string string(int index) const;
    std::string value(int index) const;

private:
    const quint8* entry(int index) const;

    const SmbiosAdditionalInfoTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosAdditionalInfo)
};

class SmbiosOnBoardDeviceExt
{
public:
    explicit SmbiosOnBoardDeviceExt(const SmbiosTable* table);

    bool isValid() const;
    bool isEnabled() const;
    std::string designation() const;
    std::string type() const;
    quint8 typeInstance() const;
    bool hasBusAddress() const;
    quint16 segmentGroupNumber() const;
    quint8 busNumber() const;
    quint8 deviceNumber() const;
    quint8 functionNumber() const;

private:
    const SmbiosOnBoardDeviceExtTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosOnBoardDeviceExt)
};

class SmbiosTpmDevice
{
public:
    explicit SmbiosTpmDevice(const SmbiosTable* table);

    bool isValid() const;
    std::string vendorId() const;
    std::string specVersion() const;
    std::string firmwareVersion() const;
    std::string description() const;
    bool isFamilyConfigurableByFirmware() const;
    bool isFamilyConfigurableBySoftware() const;
    bool isFamilyConfigurableByOem() const;

private:
    quint64 characteristics() const;

    const SmbiosTpmDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosTpmDevice)
};

class SmbiosFirmwareInventory
{
public:
    explicit SmbiosFirmwareInventory(const SmbiosTable* table);

    bool isValid() const;
    std::string name() const;
    std::string version() const;
    std::string versionFormat() const;
    std::string id() const;
    std::string idFormat() const;
    std::string releaseDate() const;
    std::string manufacturer() const;
    std::string lowestVersion() const;
    std::string state() const;
    bool isUpdatable() const;
    bool isWriteProtected() const;

    // The size of the firmware image, in bytes. Zero when the firmware does not report it.
    quint64 imageSize() const;

    // Tables of the components the firmware belongs to.
    int componentCount() const;
    quint16 componentHandle(int index) const;

private:
    const SmbiosFirmwareInventoryTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosFirmwareInventory)
};

class SmbiosProcessorInfoExt
{
public:
    explicit SmbiosProcessorInfoExt(const SmbiosTable* table);

    bool isValid() const;
    quint16 processorHandle() const;
    std::string architecture() const;

private:
    const SmbiosProcessorInfoExtTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosProcessorInfoExt)
};

#endif // BASE_SMBIOS_PARSER_H
