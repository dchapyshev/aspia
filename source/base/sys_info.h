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

#ifndef BASE_SYS_INFO_H
#define BASE_SYS_INFO_H

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

#include "base/drive_smart.h"

class SysInfo
{
public:
    struct UserGroup
    {
        QString name;
    };

    struct User
    {
        QString name;
        QString full_name;
        QString home_dir;
        QList<UserGroup> groups;
        bool disabled = false;
        bool password_expired = false;
        bool dont_expire_password = false;
        quint64 last_logon_time = 0;
    };

    struct Service
    {
        enum class Status
        {
            UNKNOWN          = 0,
            CONTINUE_PENDING = 1,
            PAUSE_PENDING    = 2,
            PAUSED           = 3,
            RUNNING          = 4,
            START_PENDING    = 5,
            STOP_PENDING     = 6,
            STOPPED          = 7
        };

        enum class StartupType
        {
            UNKNOWN      = 0,
            AUTO_START   = 1,
            DEMAND_START = 2,
            DISABLED     = 3,
            BOOT_START   = 4,
            SYSTEM_START = 5
        };

        QString name;
        QString display_name;
        QString description;
        Status status = Status::UNKNOWN;
        StartupType startup_type = StartupType::UNKNOWN;
        QString binary_path;
        QString start_name;
    };

    struct Session
    {
        enum class ConnectState
        {
            UNKNOWN       = 0,
            ACTIVE        = 1,
            CONNECTED     = 2,
            CONNECT_QUERY = 3,
            SHADOW        = 4,
            DISCONNECTED  = 5,
            IDLE          = 6,
            LISTEN        = 7,
            RESET         = 8,
            DOWN          = 9,
            INIT          = 10
        };

        quint32 id = 0;
        QString user_name;
        QString domain_name;
        QString session_name;
        QString client_name;
        ConnectState connect_state = ConnectState::UNKNOWN;
        bool locked = false;
    };

    struct Monitor
    {
        QString system_name;
        QByteArray edid;
    };

    struct VideoAdapter
    {
        QString description;
        QString adapter_string;
        QString bios_string;
        QString chip_type;
        QString dac_type;
        QString driver_date;
        QString driver_version;
        QString driver_provider;
        quint64 memory_size = 0;
    };

    struct Device
    {
        QString friendly_name;
        QString description;
        QString driver_vendor;
        QString device_id;
    };

    struct Printer
    {
        QString name;
        QString share_name;
        QString port_name;
        QString driver_name;
        bool is_default = false;
        bool is_shared = false;
        int jobs_count = 0;
    };

    struct Application
    {
        QString name;
        QString version;
        QString publisher;
    };

    struct PowerOptions
    {
        enum class PowerSource { UNKNOWN, DC_BATTERY, AC_LINE };
        enum class BatteryStatus { UNKNOWN, HIGH, LOW, CRITICAL, CHARGING, NO_BATTERY };

        struct Battery
        {
            enum State
            {
                CHARGING     = 1,
                CRITICAL     = 2,
                DISCHARGING  = 4,
                POWER_ONLINE = 8
            };

            QString device_name;
            QString manufacturer;
            QString manufacture_date;
            QString unique_id;
            QString serial_number;
            QString temperature;
            quint32 design_capacity = 0;
            QString type;
            quint32 full_charged_capacity = 0;
            quint32 depreciation = 0;
            quint32 current_capacity = 0;
            quint32 voltage = 0;
            quint32 state = 0;
        };

        PowerSource power_source = PowerSource::UNKNOWN;
        BatteryStatus battery_status = BatteryStatus::UNKNOWN;
        quint64 full_battery_life_time = 0;
        quint64 remaining_battery_life_time = 0;
        quint32 battery_life_percent = 0;
        QList<Battery> batteries;
    };

    struct PhysicalDrive
    {
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
            NVME                = 16
        };

        // Path the drive is addressed by. Its shape is specific to the operating system.
        QString path;

        QString model;
        QString serial_number;
        QString firmware_revision;
        BusType bus_type = BusType::UNKNOWN;
        quint64 size = 0;

        // Nominal media rotation rate in RPM. 0 when the drive does not report it.
        quint32 rotation_rate = 0;

        // Size of the drive cache in bytes. 0 when not reported.
        quint32 buffer_size = 0;

        bool removable = false;
        bool solid_state = false;

        // Health data of an ATA drive. Empty when the drive has no S.M.A.R.T. support or the
        // controller it sits behind does not pass the request through.
        QList<AtaSmart::Attribute> ata_smart;

        // Health data of an NVMe drive.
        std::optional<NvmeSmart::HealthInfo> nvme_smart;
    };

    static QString operatingSystemName();
    static QString operatingSystemVersion();
    static QString operatingSystemArchitecture();
    static QString operatingSystemDir();
    static QString operatingSystemKey();
    static qint64 operatingSystemInstallDate();

    static quint64 uptime();

    static QString computerName();
    static QString computerDomain();
    static QString computerWorkgroup();

    static QString processorName();
    static QString processorVendor();
    static int processorPackages();
    static int processorCores();
    static int processorThreads();

    static QByteArray smbiosDump();
    static QByteArray hardwareId();

    static QList<User> users();
    static QList<UserGroup> userGroups();

    static QList<Service> services();
    static QList<Service> drivers();

    static QList<Session> sessions();

    static QList<Monitor> monitors();
    static QList<VideoAdapter> videoAdapters();
    static QList<Device> devices();
    static QList<Printer> printers();
    static QList<Application> applications();
    static PowerOptions powerOptions();
    static QList<PhysicalDrive> physicalDrives();

private:
    Q_DISABLE_COPY_MOVE(SysInfo)
};

#endif // BASE_SYS_INFO_H
