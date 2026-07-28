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
#include <QString>
#include <QStringList>

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

QString smbiosString(const SmbiosTable* table, quint8 number);

class SmbiosBios
{
public:
    explicit SmbiosBios(const SmbiosTable* table);

    bool isValid() const;
    QString vendor() const;
    QString version() const;
    QString releaseDate() const;
    quint32 address() const;
    quint64 romSize() const;
    QString revision() const;
    QString firmwareRevision() const;
    QStringList characteristics() const;

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
    QString manufacturer() const;
    QString product() const;
    QString version() const;
    QString serialNumber() const;
    QString assetTag() const;
    QString location() const;
    QString type() const;
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
    QString manufacturer() const;
    QString version() const;
    QString serialNumber() const;
    QString assetTag() const;
    QString skuNumber() const;
    QString type() const;
    bool isLockPresent() const;
    QString bootUpState() const;
    QString powerSupplyState() const;
    QString thermalState() const;
    QString securityStatus() const;
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
    QString manufacturer() const;
    QString version() const;
    QString serialNumber() const;
    QString assetTag() const;
    QString partNumber() const;
    QString socketDesignation() const;
    QString type() const;
    QString family() const;
    QString status() const;
    QString upgrade() const;
    QString socketType() const;
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
    QString designation() const;
    QString location() const;
    QString mode() const;
    QString type() const;
    QString errorCorrectionType() const;
    QString associativity() const;
    QString currentSramType() const;
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
    QString internalDesignator() const;
    QString internalConnectorType() const;
    QString externalDesignator() const;
    QString externalConnectorType() const;
    QString type() const;

private:
    const SmbiosPortConnectorTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPortConnector)
};

class SmbiosSystemSlot
{
public:
    explicit SmbiosSystemSlot(const SmbiosTable* table);

    bool isValid() const;
    QString designation() const;
    QString type() const;
    QString dataBusWidth() const;
    QString usage() const;
    QString length() const;
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
    QString description(int index) const;
    QString type(int index) const;
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
    QString string(int index) const;

private:
    const SmbiosStringListTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosStringList)
};

class SmbiosMemoryArray
{
public:
    explicit SmbiosMemoryArray(const SmbiosTable* table);

    bool isValid() const;
    QString location() const;
    QString use() const;
    QString errorCorrection() const;
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
    QString location() const;
    QString bankLocator() const;
    QString manufacturer() const;
    QString serialNumber() const;
    QString assetTag() const;
    quint64 size() const;
    QString type() const;
    QString formFactor() const;
    QString technology() const;
    QString partNumber() const;
    QString firmwareVersion() const;

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
    QStringList typeDetail() const;
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
    QString type() const;
    QString granularity() const;
    QString operation() const;
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
    QString type() const;
    QString interfaceType() const;
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
    QString location() const;
    QString manufacturer() const;
    QString manufactureDate() const;
    QString serialNumber() const;
    QString deviceName() const;
    QString chemistry() const;
    QString sbdsVersion() const;
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
    QString description() const;
    QString location() const;
    QString status() const;

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
    QString description() const;
    QString type() const;
    QString status() const;

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
    QString status() const;

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
    QString location() const;
    QString deviceName() const;
    QString manufacturer() const;
    QString serialNumber() const;
    QString assetTag() const;
    QString modelPartNumber() const;
    QString revisionLevel() const;
    QString type() const;
    QString status() const;
    QString inputVoltageRangeSwitching() const;

    // The maximum sustained power output, in milliwatts. Zero when unknown.
    quint32 maxPowerCapacity() const;

private:
    const SmbiosPowerSupplyTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosPowerSupply)
};

class SmbiosOnBoardDeviceExt
{
public:
    explicit SmbiosOnBoardDeviceExt(const SmbiosTable* table);

    bool isValid() const;
    bool isEnabled() const;
    QString designation() const;
    QString type() const;
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
    QString vendorId() const;
    QString specVersion() const;
    QString firmwareVersion() const;
    QString description() const;
    bool isFamilyConfigurableByFirmware() const;
    bool isFamilyConfigurableBySoftware() const;
    bool isFamilyConfigurableByOem() const;

private:
    quint64 characteristics() const;

    const SmbiosTpmDeviceTable* table_;
    Q_DISABLE_COPY_MOVE(SmbiosTpmDevice)
};

#endif // BASE_SMBIOS_PARSER_H
