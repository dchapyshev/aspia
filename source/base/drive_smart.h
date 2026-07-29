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
// Parsers for the data blocks a physical drive reports about itself. Every block is produced by the
// drive, not by the operating system that fetched it, so the layouts here are the same on every
// platform: only the way the blocks are obtained is platform specific.
//

#ifndef BASE_DRIVE_SMART_H
#define BASE_DRIVE_SMART_H

#include <QByteArray>
#include <QList>

#include <string>

// Identification data of an ATA drive.
class AtaIdentify
{
public:
    // |buffer| is the 512-byte block returned by the IDENTIFY DEVICE command.
    explicit AtaIdentify(const QByteArray& buffer);
    ~AtaIdentify() = default;

    // Expected length of the data block.
    static constexpr qsizetype kDataSize = 512;

    // True when the block was long enough and the drive filled in its model.
    bool isValid() const { return valid_; }

    const std::string& model() const { return model_; }
    const std::string& serialNumber() const { return serial_number_; }
    const std::string& firmwareRevision() const { return firmware_revision_; }

    // Nominal media rotation rate in RPM. 0 when the drive does not report a rate, which is also
    // the case for solid state drives.
    quint32 rotationRate() const { return rotation_rate_; }

    // True when the drive reports itself as non-rotating media.
    bool isSolidState() const { return solid_state_; }

    // Size of the drive cache in bytes. 0 when not reported.
    quint32 bufferSize() const { return buffer_size_; }

    bool isRemovable() const { return removable_; }
    bool isSmartSupported() const { return smart_supported_; }
    bool isSmartEnabled() const { return smart_enabled_; }

private:
    std::string model_;
    std::string serial_number_;
    std::string firmware_revision_;
    quint32 rotation_rate_ = 0;
    quint32 buffer_size_ = 0;
    bool solid_state_ = false;
    bool removable_ = false;
    bool smart_supported_ = false;
    bool smart_enabled_ = false;
    bool valid_ = false;

    Q_DISABLE_COPY_MOVE(AtaIdentify)
};

// S.M.A.R.T. data of an ATA drive: a vendor defined table of normalized attributes.
class AtaSmart
{
public:
    // |attributes| is the 512-byte sector returned by SMART READ DATA, |thresholds| the sector
    // returned by SMART READ THRESHOLDS. |thresholds| may be empty when the drive refused the
    // request: every threshold then reads as zero.
    AtaSmart(const QByteArray& attributes, const QByteArray& thresholds);
    ~AtaSmart() = default;

    enum StatusFlag : quint16
    {
        // The value is expected to degrade over the life of the drive and reaching the threshold
        // indicates an upcoming failure. Attributes without the flag only collect statistics.
        STATUS_FLAG_PRE_FAILURE     = 0x0001,
        STATUS_FLAG_ONLINE          = 0x0002,
        STATUS_FLAG_PERFORMANCE     = 0x0004,
        STATUS_FLAG_ERROR_RATE      = 0x0008,
        STATUS_FLAG_EVENT_COUNT     = 0x0010,
        STATUS_FLAG_SELF_PRESERVING = 0x0020
    };

    struct Attribute
    {
        // Vendor specific meaning, but the widely used identifiers are the same across vendors.
        quint8 id = 0;

        // Bit mask of StatusFlag.
        quint16 status_flags = 0;

        // Normalized current and lifetime worst values. Higher is better.
        quint8 value = 0;
        quint8 worst_value = 0;

        // |value| at or below the threshold means the attribute failed. 0 means the drive does not
        // define a threshold for the attribute.
        quint8 threshold = 0;

        // 48-bit vendor specific counter. Its meaning depends on |id| and on the drive model.
        quint64 raw = 0;
    };

    // True when the attributes sector was long enough and contained at least one used entry.
    bool isValid() const { return !attributes_.isEmpty(); }

    const QList<Attribute>& attributes() const { return attributes_; }

private:
    QList<Attribute> attributes_;

    Q_DISABLE_COPY_MOVE(AtaSmart)
};

// Health information of an NVMe drive. Unlike an ATA drive, which reports a table of normalized
// attributes, an NVMe drive reports a fixed set of named counters.
class NvmeSmart
{
public:
    // |health_log| is the 512-byte SMART / Health Information log page (log identifier 02h).
    explicit NvmeSmart(const QByteArray& health_log);
    ~NvmeSmart() = default;

    // Identifier of the log page holding the data this class parses. Callers use it to build the
    // platform specific request.
    static constexpr quint8 kHealthLogPageId = 0x02;

    // Expected length of the log page.
    static constexpr qsizetype kHealthLogSize = 512;

    // Number of temperature sensors the log page can report.
    static constexpr int kTemperatureSensorCount = 8;

    enum CriticalWarning : quint8
    {
        CRITICAL_WARNING_SPARE_BELOW_THRESHOLD   = 0x01,
        CRITICAL_WARNING_TEMPERATURE             = 0x02,
        CRITICAL_WARNING_RELIABILITY_DEGRADED    = 0x04,
        CRITICAL_WARNING_READ_ONLY               = 0x08,
        CRITICAL_WARNING_VOLATILE_BACKUP_FAILED  = 0x10,
        CRITICAL_WARNING_PERSISTENT_MEMORY_ERROR = 0x20
    };

    struct HealthInfo
    {
        // Bit mask of CriticalWarning.
        quint8 critical_warning = 0;

        // Temperatures are in Kelvin as reported by the drive. 0 means not reported.
        quint16 composite_temperature = 0;
        quint16 temperature_sensor[kTemperatureSensorCount] = {};

        // Percentage of the remaining spare capacity and the threshold below which the drive
        // reports CRITICAL_WARNING_SPARE_BELOW_THRESHOLD.
        quint8 available_spare = 0;
        quint8 available_spare_threshold = 0;

        // Estimated wear of the drive in percent of its rated endurance. May exceed 100.
        quint8 percentage_used = 0;

        // The counters below are 128-bit at the source and are truncated to their low 64 bits.

        // One data unit is 1000 blocks of 512 bytes.
        quint64 data_units_read = 0;
        quint64 data_units_written = 0;

        quint64 host_read_commands = 0;
        quint64 host_write_commands = 0;
        quint64 controller_busy_time = 0; // minutes
        quint64 power_cycles = 0;
        quint64 power_on_hours = 0;
        quint64 unsafe_shutdowns = 0;
        quint64 media_errors = 0;
        quint64 error_log_entries = 0;

        // Time the drive spent above its warning and critical temperature thresholds.
        quint32 warning_temperature_time = 0; // minutes
        quint32 critical_temperature_time = 0; // minutes
    };

    // True when the log page was long enough to be parsed.
    bool isValid() const { return valid_; }

    const HealthInfo& healthInfo() const { return health_info_; }

private:
    HealthInfo health_info_;
    bool valid_ = false;

    Q_DISABLE_COPY_MOVE(NvmeSmart)
};

#endif // BASE_DRIVE_SMART_H
