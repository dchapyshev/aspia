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

#include "base/drive_smart.h"

#include <QtEndian>

namespace {

// Byte offsets of the fields inside the ATA IDENTIFY DEVICE block. The block is defined as an array
// of 16-bit words, the word number of each field is given in the comment.
constexpr qsizetype kGeneralConfiguration = 0; // 0
constexpr qsizetype kSerialNumber = 20; // 10-19
constexpr qsizetype kCacheSize = 42; // 21
constexpr qsizetype kFirmwareRevision = 46; // 23-26
constexpr qsizetype kModelNumber = 54; // 27-46
constexpr qsizetype kCommandSetSupported = 164; // 82
constexpr qsizetype kCommandSetEnabled = 170; // 85
constexpr qsizetype kRotationRate = 434; // 217

constexpr qsizetype kSerialNumberSize = 20;
constexpr qsizetype kFirmwareRevisionSize = 8;
constexpr qsizetype kModelNumberSize = 40;

constexpr quint16 kRemovableMedia = 0x0080;
constexpr quint16 kSmartSupported = 0x0001;

// Rotation rates outside of this range are either reserved or mean "not reported".
constexpr quint16 kNonRotatingMedia = 0x0001;
constexpr quint16 kMinRotationRate = 0x0401;
constexpr quint16 kMaxRotationRate = 0xFFFE;

// The drive cache size is reported in 512-byte units.
constexpr quint32 kCacheSizeUnit = 512;

// Layout of the S.M.A.R.T. data and threshold tables. Both tables hold the same number of fixed
// size entries and start right after the two byte data structure revision number.
constexpr qsizetype kTableSectorSize = 512;
constexpr qsizetype kTableEntrySize = 12;
constexpr qsizetype kTableFirstEntry = 2;
constexpr int kTableEntryCount = 30;

// Byte offsets of the fields inside the NVMe health log page.
constexpr qsizetype kCriticalWarning = 0;
constexpr qsizetype kCompositeTemperature = 1;
constexpr qsizetype kAvailableSpare = 3;
constexpr qsizetype kAvailableSpareThreshold = 4;
constexpr qsizetype kPercentageUsed = 5;
constexpr qsizetype kDataUnitsRead = 32;
constexpr qsizetype kDataUnitsWritten = 48;
constexpr qsizetype kHostReadCommands = 64;
constexpr qsizetype kHostWriteCommands = 80;
constexpr qsizetype kControllerBusyTime = 96;
constexpr qsizetype kPowerCycles = 112;
constexpr qsizetype kPowerOnHours = 128;
constexpr qsizetype kUnsafeShutdowns = 144;
constexpr qsizetype kMediaErrors = 160;
constexpr qsizetype kErrorLogEntries = 176;
constexpr qsizetype kWarningTemperatureTime = 192;
constexpr qsizetype kCriticalTemperatureTime = 196;
constexpr qsizetype kTemperatureSensor = 200;

//--------------------------------------------------------------------------------------------------
// Strings of the identification block are stored as 16-bit words with the characters of each word
// in reverse order.
QString readString(const quint8* data, qsizetype size)
{
    QByteArray result(size, 0);

    for (qsizetype i = 0; i < size; i += 2)
    {
        result[i] = static_cast<char>(data[i + 1]);
        result[i + 1] = static_cast<char>(data[i]);
    }

    const qsizetype end = result.indexOf('\0');
    if (end >= 0)
        result.truncate(end);

    return QString::fromLatin1(result).trimmed();
}

//--------------------------------------------------------------------------------------------------
// Reads the 48-bit raw value of an attribute table entry.
quint64 readRaw(const quint8* data)
{
    quint64 result = 0;

    for (int i = 0; i < 6; ++i)
        result |= static_cast<quint64>(data[i]) << (8 * i);

    return result;
}

//--------------------------------------------------------------------------------------------------
quint8 findThreshold(const QByteArray& thresholds, quint8 attribute_id)
{
    if (thresholds.size() < kTableSectorSize)
        return 0;

    const quint8* entry =
        reinterpret_cast<const quint8*>(thresholds.constData()) + kTableFirstEntry;

    // The threshold table is not required to list the attributes in the same order as the data
    // table, so the entry has to be looked up by identifier.
    for (int i = 0; i < kTableEntryCount; ++i, entry += kTableEntrySize)
    {
        if (entry[0] == attribute_id)
            return entry[1];
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
// Reads the low 64 bits of a 128-bit little endian counter of the health log page.
quint64 readCounter(const quint8* data)
{
    return qFromLittleEndian<quint64>(data);
}

} // namespace

//--------------------------------------------------------------------------------------------------
AtaIdentify::AtaIdentify(const QByteArray& buffer)
{
    if (buffer.size() < kDataSize)
        return;

    const quint8* data = reinterpret_cast<const quint8*>(buffer.constData());

    model_ = readString(data + kModelNumber, kModelNumberSize);
    if (model_.isEmpty())
        return;

    serial_number_ = readString(data + kSerialNumber, kSerialNumberSize);
    firmware_revision_ = readString(data + kFirmwareRevision, kFirmwareRevisionSize);

    const quint16 rotation_rate = qFromLittleEndian<quint16>(data + kRotationRate);
    if (rotation_rate == kNonRotatingMedia)
        solid_state_ = true;
    else if (rotation_rate >= kMinRotationRate && rotation_rate <= kMaxRotationRate)
        rotation_rate_ = rotation_rate;

    buffer_size_ = qFromLittleEndian<quint16>(data + kCacheSize) * kCacheSizeUnit;
    removable_ = qFromLittleEndian<quint16>(data + kGeneralConfiguration) & kRemovableMedia;
    smart_supported_ = qFromLittleEndian<quint16>(data + kCommandSetSupported) & kSmartSupported;
    smart_enabled_ = smart_supported_ &&
        (qFromLittleEndian<quint16>(data + kCommandSetEnabled) & kSmartSupported);

    valid_ = true;
}

//--------------------------------------------------------------------------------------------------
AtaSmart::AtaSmart(const QByteArray& attributes, const QByteArray& thresholds)
{
    if (attributes.size() < kTableSectorSize)
        return;

    const quint8* entry =
        reinterpret_cast<const quint8*>(attributes.constData()) + kTableFirstEntry;

    for (int i = 0; i < kTableEntryCount; ++i, entry += kTableEntrySize)
    {
        // Identifier 0 marks an unused table slot.
        if (!entry[0])
            continue;

        Attribute attribute;

        attribute.id = entry[0];
        attribute.status_flags = qFromLittleEndian<quint16>(entry + 1);
        attribute.value = entry[3];
        attribute.worst_value = entry[4];
        attribute.raw = readRaw(entry + 5);
        attribute.threshold = findThreshold(thresholds, attribute.id);

        attributes_.append(attribute);
    }
}

//--------------------------------------------------------------------------------------------------
NvmeSmart::NvmeSmart(const QByteArray& health_log)
{
    if (health_log.size() < kHealthLogSize)
        return;

    const quint8* log = reinterpret_cast<const quint8*>(health_log.constData());

    health_info_.critical_warning = log[kCriticalWarning];
    health_info_.composite_temperature = qFromLittleEndian<quint16>(log + kCompositeTemperature);

    for (int i = 0; i < kTemperatureSensorCount; ++i)
    {
        health_info_.temperature_sensor[i] =
            qFromLittleEndian<quint16>(log + kTemperatureSensor + (i * sizeof(quint16)));
    }

    health_info_.available_spare = log[kAvailableSpare];
    health_info_.available_spare_threshold = log[kAvailableSpareThreshold];
    health_info_.percentage_used = log[kPercentageUsed];

    health_info_.data_units_read = readCounter(log + kDataUnitsRead);
    health_info_.data_units_written = readCounter(log + kDataUnitsWritten);
    health_info_.host_read_commands = readCounter(log + kHostReadCommands);
    health_info_.host_write_commands = readCounter(log + kHostWriteCommands);
    health_info_.controller_busy_time = readCounter(log + kControllerBusyTime);
    health_info_.power_cycles = readCounter(log + kPowerCycles);
    health_info_.power_on_hours = readCounter(log + kPowerOnHours);
    health_info_.unsafe_shutdowns = readCounter(log + kUnsafeShutdowns);
    health_info_.media_errors = readCounter(log + kMediaErrors);
    health_info_.error_log_entries = readCounter(log + kErrorLogEntries);

    health_info_.warning_temperature_time = qFromLittleEndian<quint32>(log + kWarningTemperatureTime);
    health_info_.critical_temperature_time =
        qFromLittleEndian<quint32>(log + kCriticalTemperatureTime);

    valid_ = true;
}
