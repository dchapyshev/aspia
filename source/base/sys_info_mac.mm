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

#include "base/sys_info.h"

#include "base/logging.h"
#include "base/smbios.h"

#include <QHash>
#include <QProcess>

#include <utility>

#include <cups/cups.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/ps/IOPSKeys.h>
#import <IOKit/ps/IOPowerSources.h>

namespace {

//--------------------------------------------------------------------------------------------------
// Reads an unsigned integer IORegistry property. PCI nubs report ids as little-endian CFData, USB
// nubs as CFNumber, so both encodings are accepted.
quint32 numberProperty(io_service_t service, CFStringRef key)
{
    CFTypeRef property =
        IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, kNilOptions);
    if (!property)
        return 0;

    quint32 value = 0;
    if (CFGetTypeID(property) == CFDataGetTypeID())
    {
        CFDataRef data = static_cast<CFDataRef>(property);
        CFIndex length = CFDataGetLength(data);
        if (length > static_cast<CFIndex>(sizeof(value)))
            length = sizeof(value);
        CFDataGetBytes(data, CFRangeMake(0, length), reinterpret_cast<UInt8*>(&value));
    }
    else if (CFGetTypeID(property) == CFNumberGetTypeID())
    {
        CFNumberGetValue(static_cast<CFNumberRef>(property), kCFNumberSInt32Type, &value);
    }

    CFRelease(property);
    return value;
}

//--------------------------------------------------------------------------------------------------
QString stringProperty(io_service_t service, CFStringRef key)
{
    CFTypeRef property =
        IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, kNilOptions);
    if (!property)
        return QString();

    QString result;
    if (CFGetTypeID(property) == CFStringGetTypeID())
        result = QString::fromCFString(static_cast<CFStringRef>(property));

    CFRelease(property);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool boolProperty(io_service_t service, CFStringRef key)
{
    CFTypeRef property =
        IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, kNilOptions);
    if (!property)
        return false;

    bool result = false;
    if (CFGetTypeID(property) == CFBooleanGetTypeID())
        result = CFBooleanGetValue(static_cast<CFBooleanRef>(property));

    CFRelease(property);
    return result;
}

//--------------------------------------------------------------------------------------------------
// Maps a PCI vendor id to a display name (the vendors present in Apple Silicon Macs).
QString pciVendorName(quint32 vendor_id)
{
    switch (vendor_id & 0xFFFF)
    {
        case 0x106b: return "Apple";
        case 0x14e4: return "Broadcom";
        default:     return QString();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    return "Mac OS X";
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
    return QString("%1.%2.%3")
        .arg(version.majorVersion).arg(version.minorVersion).arg(version.patchVersion);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
    // "arm64" on Apple Silicon, "x86_64" on Intel.
    char machine[64] = {};
    size_t size = sizeof(machine);
    if (sysctlbyname("hw.machine", machine, &size, nullptr, 0) != 0)
        return QString();

    return QString::fromLocal8Bit(machine);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemDir()
{
    return "/System";
}

//--------------------------------------------------------------------------------------------------
// static
quint64 SysInfo::uptime()
{
    // Seconds since boot (including time spent asleep), derived from the kernel boot timestamp.
    struct timeval boottime;
    size_t size = sizeof(boottime);
    if (sysctlbyname("kern.boottime", &boottime, &size, nullptr, 0) != 0)
        return 0;

    const time_t now = time(nullptr);
    if (now <= boottime.tv_sec)
        return 0;

    return static_cast<quint64>(now - boottime.tv_sec);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    char buffer[256];
    if (gethostname(buffer, std::size(buffer)) < 0)
        return QString();

    return QString::fromLocal8Bit(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerDomain()
{
    char host_name[256];
    if (gethostname(host_name, sizeof(host_name)) != 0)
    {
        PLOG(ERROR) << "gethostname failed";
        return QString();
    }
    host_name[sizeof(host_name) - 1] = 0;

    // Resolve the canonical FQDN and take everything after the first label as the DNS domain (the
    // closest analogue of the Windows primary DNS suffix).
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;

    struct addrinfo* info = nullptr;
    if (getaddrinfo(host_name, nullptr, &hints, &info) != 0 || !info)
        return QString();

    QString fqdn = QString::fromLocal8Bit(info->ai_canonname);
    freeaddrinfo(info);

    const int dot = fqdn.indexOf('.');
    if (dot < 0)
        return QString();

    return fqdn.mid(dot + 1);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    // The SMB/NetBIOS workgroup is stored in the system SMB server preferences (default "WORKGROUP").
    NSURL* url = [NSURL fileURLWithPath:
        @"/Library/Preferences/SystemConfiguration/com.apple.smb.server.plist"];
    NSDictionary* prefs = [NSDictionary dictionaryWithContentsOfURL:url error:nil];

    NSString* workgroup = [prefs objectForKey:@"Workgroup"];
    if ([workgroup isKindOfClass:[NSString class]] && workgroup.length > 0)
        return QString::fromUtf8(workgroup.UTF8String);

    // Not explicitly configured: the SMB server advertises "WORKGROUP" by default.
    return "WORKGROUP";
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorPackages()
{
    // Number of physical CPU packages (sockets).
    int packages = 0;
    size_t size = sizeof(packages);
    if (sysctlbyname("hw.packages", &packages, &size, nullptr, 0) != 0 || packages <= 0)
        return 1;

    return packages;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    // Number of physical cores (hyper-threading siblings collapsed).
    int cores = 0;
    size_t size = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) != 0 || cores <= 0)
        return 1;

    return cores;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorThreads()
{
    // Number of logical processors (physical cores times threads per core).
    int threads = 0;
    size_t size = sizeof(threads);
    if (sysctlbyname("hw.logicalcpu", &threads, &size, nullptr, 0) != 0 || threads <= 0)
        return 1;

    return threads;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray SysInfo::smbiosDump()
{
    // The SMBIOS table is exposed by the AppleSMBIOS IOService. Apple Silicon Macs have no SMBIOS,
    // so that service (and therefore this method) yields nothing there.
    io_service_t service =
        IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSMBIOS"));
    if (!service)
        return QByteArray();

    auto readData = [service](CFStringRef key) -> QByteArray
    {
        QByteArray data;
        CFTypeRef property = IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0);
        if (property)
        {
            if (CFGetTypeID(property) == CFDataGetTypeID())
            {
                CFDataRef cf_data = static_cast<CFDataRef>(property);
                data = QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(cf_data)),
                                  static_cast<qsizetype>(CFDataGetLength(cf_data)));
            }
            CFRelease(property);
        }
        return data;
    };

    // Raw structure table and the entry point (which carries the SMBIOS version).
    const QByteArray table = readData(CFSTR("SMBIOS"));
    const QByteArray entry_point = readData(CFSTR("SMBIOS-EPS"));

    IOObjectRelease(service);

    if (table.isEmpty() || table.size() > static_cast<qsizetype>(kSmbiosMaxDataSize))
    {
        LOG(ERROR) << "Unable to read SMBIOS table data";
        return QByteArray();
    }

    // 32-bit "_SM_" (version at bytes 6/7) or 64-bit "_SM3_" (bytes 7/8).
    quint8 major_version = 0;
    quint8 minor_version = 0;
    if (entry_point.startsWith("_SM3_") && entry_point.size() >= 9)
    {
        major_version = static_cast<quint8>(entry_point[7]);
        minor_version = static_cast<quint8>(entry_point[8]);
    }
    else if (entry_point.startsWith("_SM_") && entry_point.size() >= 8)
    {
        major_version = static_cast<quint8>(entry_point[6]);
        minor_version = static_cast<quint8>(entry_point[7]);
    }

    // Assemble the SmbiosDump layout the parser expects: an 8-byte header (calling method, version,
    // DMI revision, table length) followed by the raw table data.
    const quint32 length = static_cast<quint32>(table.size());

    QByteArray result;
    result.reserve(static_cast<qsizetype>(sizeof(quint8) * 4 + sizeof(quint32)) + table.size());
    result.append(static_cast<char>(0));             // used_20_calling_method
    result.append(static_cast<char>(major_version)); // smbios_major_version
    result.append(static_cast<char>(minor_version)); // smbios_minor_version
    result.append(static_cast<char>(0));             // dmi_revision
    result.append(reinterpret_cast<const char*>(&length), sizeof(length));
    result.append(table);

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::User> SysInfo::users()
{
    QList<User> result;

    setpwent();

    for (struct passwd* pw = getpwent(); pw != nullptr; pw = getpwent())
    {
        User user;

        if (pw->pw_name)
            user.name = QString::fromUtf8(pw->pw_name);

        // The GECOS field stores the full name in its first comma-separated component.
        if (pw->pw_gecos && pw->pw_gecos[0] != '\0')
        {
            const QByteArray gecos(pw->pw_gecos);
            const int comma = gecos.indexOf(',');
            user.full_name = QString::fromUtf8(comma >= 0 ? gecos.left(comma) : gecos);
        }

        if (pw->pw_dir)
            user.home_dir = QString::fromUtf8(pw->pw_dir);

        // Treat an account whose login shell denies interactive login as disabled.
        if (pw->pw_shell)
        {
            const QByteArray shell(pw->pw_shell);
            user.disabled = shell.endsWith("/nologin") || shell.endsWith("/false");
        }

        // Primary and supplementary groups the user belongs to.
        if (pw->pw_name)
        {
            int count = 0;
            getgrouplist(pw->pw_name, static_cast<int>(pw->pw_gid), nullptr, &count);
            if (count > 0)
            {
                QList<int> gids(count);
                if (getgrouplist(pw->pw_name, static_cast<int>(pw->pw_gid), gids.data(), &count) != -1)
                {
                    for (int gid : std::as_const(gids))
                    {
                        struct group* gr = getgrgid(static_cast<gid_t>(gid));
                        if (gr && gr->gr_name)
                        {
                            UserGroup group;
                            group.name = QString::fromUtf8(gr->gr_name);
                            user.groups.append(std::move(group));
                        }
                    }
                }
            }
        }

        // Last login time from the utmpx database. macOS keeps no shadow database (password aging
        // lives in the directory service's policy), so the password flags are left at their defaults.
        struct lastlogx entry = {};
        if (getlastlogx(pw->pw_uid, &entry) && entry.ll_tv.tv_sec != 0)
            user.last_logon_time = static_cast<quint64>(entry.ll_tv.tv_sec);

        result.append(std::move(user));
    }

    endpwent();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::UserGroup> SysInfo::userGroups()
{
    QList<UserGroup> result;

    setgrent();

    for (struct group* gr = getgrent(); gr != nullptr; gr = getgrent())
    {
        UserGroup group;

        if (gr->gr_name)
            group.name = QString::fromUtf8(gr->gr_name);

        result.append(std::move(group));
    }

    endgrent();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::services()
{
    QList<Service> result;

    // macOS has no public API to enumerate launchd jobs, so read the running state from
    // "launchctl list" (label -> has a running PID) and the configuration from the daemon plists.
    // Its output is "PID<TAB>Status<TAB>Label"; a numeric PID column means the job is running.
    QHash<QString, bool> running;
    {
        QProcess process;
        process.start("launchctl", QStringList() << "list");
        if (process.waitForFinished(5000))
        {
            const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
            for (const QByteArray& line : lines)
            {
                const QList<QByteArray> columns = line.split('\t');
                if (columns.size() < 3)
                    continue;

                const QString label = QString::fromUtf8(columns[2]).trimmed();
                if (label.isEmpty() || label == "Label") // Skip the header row.
                    continue;

                bool has_pid = false;
                columns[0].trimmed().toInt(&has_pid);
                running.insert(label, has_pid);
            }
        }
    }

    for (NSString* dir in @[ @"/System/Library/LaunchDaemons", @"/Library/LaunchDaemons" ])
    {
        NSArray* files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil];
        for (NSString* file in files)
        {
            if (![file hasSuffix:@".plist"])
                continue;

            NSString* path = [dir stringByAppendingPathComponent:file];
            NSDictionary* plist =
                [NSDictionary dictionaryWithContentsOfURL:[NSURL fileURLWithPath:path] error:nil];

            NSString* label = plist[@"Label"];
            if (![label isKindOfClass:[NSString class]])
                continue;

            Service service;
            service.name = QString::fromUtf8(label.UTF8String);
            service.display_name = service.name;

            // The launched executable: "Program", otherwise the first "ProgramArguments" element.
            NSString* program = plist[@"Program"];
            if ([program isKindOfClass:[NSString class]])
            {
                service.binary_path = QString::fromUtf8(program.UTF8String);
            }
            else
            {
                NSArray* arguments = plist[@"ProgramArguments"];
                if ([arguments isKindOfClass:[NSArray class]] && arguments.count > 0 &&
                    [arguments[0] isKindOfClass:[NSString class]])
                {
                    service.binary_path = QString::fromUtf8([arguments[0] UTF8String]);
                }
            }

            // Daemons run as root unless a UserName is given.
            NSString* user = plist[@"UserName"];
            service.start_name =
                [user isKindOfClass:[NSString class]] ? QString::fromUtf8(user.UTF8String) : "root";

            NSNumber* disabled = plist[@"Disabled"];
            NSNumber* run_at_load = plist[@"RunAtLoad"];
            if ([disabled isKindOfClass:[NSNumber class]] && disabled.boolValue)
                service.startup_type = Service::StartupType::DISABLED;
            else if ([run_at_load isKindOfClass:[NSNumber class]] && run_at_load.boolValue)
                service.startup_type = Service::StartupType::AUTO_START;
            else
                service.startup_type = Service::StartupType::DEMAND_START;

            // Loaded with a PID -> running; loaded without one or not loaded at all -> stopped.
            service.status = running.value(service.name, false) ? Service::Status::RUNNING
                                                                : Service::Status::STOPPED;

            result.append(std::move(service));
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::drivers()
{
    QList<Service> result;

    // Loaded kernel extensions, listed by kmutil (the modern replacement for kextstat). Each data
    // row is: Index Refs Address Size Wired Name (Version) UUID <Linked Against>.
    QProcess process;
    process.start("/usr/bin/kmutil", QStringList() << "showloaded");
    if (!process.waitForFinished(15000))
        return result;

    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& line : lines)
    {
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 7)
            continue;

        bool is_data_row = false;
        fields[0].toInt(&is_data_row);
        if (!is_data_row) // Skip the header and any wrapped lines.
            continue;

        Service driver;
        driver.name = QString::fromUtf8(fields[5]);
        driver.display_name = driver.name;
        driver.description = QString::fromUtf8(fields[6]).remove('(').remove(')');
        driver.status = Service::Status::RUNNING;
        driver.startup_type = Service::StartupType::SYSTEM_START;

        result.append(std::move(driver));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Session> SysInfo::sessions()
{
    QList<Session> result;

    // Active login sessions from the utmp database: the local console, terminals and remote logins.
    setutxent();

    for (struct utmpx* entry = getutxent(); entry != nullptr; entry = getutxent())
    {
        if (entry->ut_type != USER_PROCESS)
            continue;

        Session session;
        session.id = static_cast<quint32>(entry->ut_pid);
        session.user_name = QString::fromUtf8(entry->ut_user);
        session.session_name = QString::fromUtf8(entry->ut_line);
        session.client_name = QString::fromUtf8(entry->ut_host);
        session.connect_state = Session::ConnectState::ACTIVE;

        result.append(std::move(session));
    }

    endutxent();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Monitor> SysInfo::monitors()
{
    QList<Monitor> result;

    // An external display exposes its raw EDID under the "EDID" property of the port/transport node
    // that carries it. The node class depends on the connector (HDMI, DisplayPort, USB-C), so rather
    // than match a class, walk the whole registry and collect every entry that has an EDID. The
    // built-in Apple Silicon panel has no consumer EDID and never appears here.
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IORegistryCreateIterator(kIOMainPortDefault, kIOServicePlane,
                                 kIORegistryIterateRecursively, &iterator) != KERN_SUCCESS)
    {
        return result;
    }

    io_registry_entry_t entry;
    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
    {
        CFDataRef edid_data = static_cast<CFDataRef>(
            IORegistryEntryCreateCFProperty(entry, CFSTR("EDID"), kCFAllocatorDefault, kNilOptions));
        if (edid_data)
        {
            Monitor monitor;
            monitor.edid = QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(edid_data)),
                                      CFDataGetLength(edid_data));

            io_name_t name = {};
            if (IORegistryEntryGetName(entry, name) == KERN_SUCCESS)
                monitor.system_name = QString::fromUtf8(name);

            result.append(std::move(monitor));
            CFRelease(edid_data);
        }

        IOObjectRelease(entry);
    }

    IOObjectRelease(iterator);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::VideoAdapter> SysInfo::videoAdapters()
{
    QList<VideoAdapter> result;

    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(
            kIOMainPortDefault, IOServiceMatching("IOGPU"), &iterator) != KERN_SUCCESS)
    {
        return result;
    }

    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
    {
        const QString model = stringProperty(service, CFSTR("model"));
        if (!model.isEmpty())
        {
            // The integrated Apple GPU shares unified system memory, so memory_size is left unset.
            VideoAdapter adapter;
            adapter.description = model;
            adapter.adapter_string = model;
            adapter.driver_provider = pciVendorName(numberProperty(service, CFSTR("vendor-id")));

            result.append(std::move(adapter));
        }

        IOObjectRelease(service);
    }

    IOObjectRelease(iterator);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Device> SysInfo::devices()
{
    QList<Device> result;
    io_iterator_t iterator = IO_OBJECT_NULL;

    // PCI devices: ids are little-endian CFData; the id is built in the Windows style, e.g.
    // "PCI\VEN_8086&DEV_9A49&SUBSYS_1EBF1043&REV_01".
    if (IOServiceGetMatchingServices(
            kIOMainPortDefault, IOServiceMatching("IOPCIDevice"), &iterator) == KERN_SUCCESS)
    {
        io_service_t service;
        while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
        {
            const quint32 vendor = numberProperty(service, CFSTR("vendor-id"));

            Device device;
            device.friendly_name = stringProperty(service, CFSTR("model"));
            if (device.friendly_name.isEmpty())
                device.friendly_name = stringProperty(service, CFSTR("IOName"));
            device.driver_vendor = pciVendorName(vendor);
            device.device_id = QString("PCI\\VEN_%1&DEV_%2&SUBSYS_%3%4&REV_%5")
                .arg(vendor & 0xFFFF, 4, 16, QChar('0'))
                .arg(numberProperty(service, CFSTR("device-id")) & 0xFFFF, 4, 16, QChar('0'))
                .arg(numberProperty(service, CFSTR("subsystem-id")) & 0xFFFF, 4, 16, QChar('0'))
                .arg(numberProperty(service, CFSTR("subsystem-vendor-id")) & 0xFFFF, 4, 16, QChar('0'))
                .arg(numberProperty(service, CFSTR("revision-id")) & 0xFF, 2, 16, QChar('0'))
                .toUpper();

            result.append(std::move(device));
            IOObjectRelease(service);
        }

        IOObjectRelease(iterator);
    }

    // USB devices from the IOUSBHost stack. Id example: "USB\VID_046D&PID_C539&REV_3906".
    if (IOServiceGetMatchingServices(
            kIOMainPortDefault, IOServiceMatching("IOUSBHostDevice"), &iterator) == KERN_SUCCESS)
    {
        io_service_t service;
        while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
        {
            const quint32 vid = numberProperty(service, CFSTR("idVendor"));

            Device device;
            device.friendly_name = stringProperty(service, CFSTR("USB Product Name"));
            device.driver_vendor = stringProperty(service, CFSTR("USB Vendor Name"));
            if (vid != 0)
            {
                device.device_id = QString("USB\\VID_%1&PID_%2&REV_%3")
                    .arg(vid & 0xFFFF, 4, 16, QChar('0'))
                    .arg(numberProperty(service, CFSTR("idProduct")) & 0xFFFF, 4, 16, QChar('0'))
                    .arg(numberProperty(service, CFSTR("bcdDevice")) & 0xFFFF, 4, 16, QChar('0'))
                    .toUpper();
            }

            result.append(std::move(device));
            IOObjectRelease(service);
        }

        IOObjectRelease(iterator);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Printer> SysInfo::printers()
{
    QList<Printer> result;

    cups_dest_t* dests = nullptr;
    const int dest_count = cupsGetDests2(CUPS_HTTP_DEFAULT, &dests);

    for (int i = 0; i < dest_count; ++i)
    {
        const cups_dest_t& dest = dests[i];

        Printer printer;
        printer.name = QString::fromUtf8(dest.name);
        printer.is_default = dest.is_default != 0;

        if (const char* uri = cupsGetOption("device-uri", dest.num_options, dest.options))
            printer.port_name = QString::fromUtf8(uri);
        if (const char* model = cupsGetOption("printer-make-and-model", dest.num_options, dest.options))
            printer.driver_name = QString::fromUtf8(model);
        if (const char* shared = cupsGetOption("printer-is-shared", dest.num_options, dest.options))
            printer.is_shared = (QString::fromUtf8(shared) == "true");

        // Count the queued/processing jobs for this destination.
        cups_job_t* jobs = nullptr;
        const int job_count =
            cupsGetJobs2(CUPS_HTTP_DEFAULT, &jobs, dest.name, 0, CUPS_WHICHJOBS_ACTIVE);
        if (job_count > 0)
            printer.jobs_count = job_count;
        cupsFreeJobs(job_count, jobs);

        result.append(std::move(printer));
    }

    cupsFreeDests(dest_count, dests);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
SysInfo::PowerOptions SysInfo::powerOptions()
{
    PowerOptions result;

    // Overall power state from the Power Sources API, which reports a uniform 0-100 percentage.
    CFTypeRef ps_info = IOPSCopyPowerSourcesInfo();
    CFArrayRef ps_list = ps_info ? IOPSCopyPowerSourcesList(ps_info) : nullptr;
    bool has_battery = false;

    for (CFIndex i = 0; ps_list && i < CFArrayGetCount(ps_list); ++i)
    {
        CFDictionaryRef description =
            IOPSGetPowerSourceDescription(ps_info, CFArrayGetValueAtIndex(ps_list, i));
        if (!description)
            continue;

        has_battery = true;

        CFStringRef state = static_cast<CFStringRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSPowerSourceStateKey)));
        const bool ac_online = state &&
            CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo;

        CFBooleanRef charging = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSIsChargingKey)));
        const bool is_charging = charging && CFBooleanGetValue(charging);

        int current = 0;
        int max = 0;
        CFNumberRef current_ref = static_cast<CFNumberRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSCurrentCapacityKey)));
        CFNumberRef max_ref = static_cast<CFNumberRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSMaxCapacityKey)));
        if (current_ref)
            CFNumberGetValue(current_ref, kCFNumberIntType, &current);
        if (max_ref)
            CFNumberGetValue(max_ref, kCFNumberIntType, &max);
        if (max > 0)
            result.battery_life_percent = static_cast<quint32>(current * 100 / max);

        result.power_source = ac_online ? PowerOptions::PowerSource::AC_LINE
                                        : PowerOptions::PowerSource::DC_BATTERY;

        if (is_charging)
            result.battery_status = PowerOptions::BatteryStatus::CHARGING;
        else if (result.battery_life_percent <= 5)
            result.battery_status = PowerOptions::BatteryStatus::CRITICAL;
        else if (result.battery_life_percent <= 33)
            result.battery_status = PowerOptions::BatteryStatus::LOW;
        else
            result.battery_status = PowerOptions::BatteryStatus::HIGH;

        // Minutes until empty (on battery) or until full (charging); -1 means "still calculating".
        int minutes = 0;
        CFNumberRef time_ref = static_cast<CFNumberRef>(CFDictionaryGetValue(
            description, is_charging ? CFSTR(kIOPSTimeToFullChargeKey)
                                     : CFSTR(kIOPSTimeToEmptyKey)));
        if (time_ref)
            CFNumberGetValue(time_ref, kCFNumberIntType, &minutes);
        if (minutes > 0)
            result.remaining_battery_life_time = static_cast<quint64>(minutes) * 60;
    }

    if (ps_list)
        CFRelease(ps_list);
    if (ps_info)
        CFRelease(ps_info);

    if (!has_battery)
    {
        result.battery_status = PowerOptions::BatteryStatus::NO_BATTERY;
        result.power_source = PowerOptions::PowerSource::AC_LINE;
        return result;
    }

    // Detailed telemetry from the battery gas gauge.
    io_service_t battery =
        IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (battery != IO_OBJECT_NULL)
    {
        const bool external = boolProperty(battery, CFSTR("ExternalConnected"));
        const bool charging = boolProperty(battery, CFSTR("IsCharging"));

        PowerOptions::Battery info;
        info.device_name = stringProperty(battery, CFSTR("DeviceName"));
        info.manufacturer = stringProperty(battery, CFSTR("Manufacturer"));
        info.serial_number = stringProperty(battery, CFSTR("Serial"));
        info.unique_id = info.serial_number;
        info.voltage = numberProperty(battery, CFSTR("Voltage")); // mV

        // Charge figures in mAh; the AppleRaw* keys hold the true full/current charge (the plain
        // MaxCapacity/CurrentCapacity keys are percentages on Apple Silicon).
        info.design_capacity = numberProperty(battery, CFSTR("DesignCapacity"));
        info.full_charged_capacity = numberProperty(battery, CFSTR("AppleRawMaxCapacity"));
        info.current_capacity = numberProperty(battery, CFSTR("AppleRawCurrentCapacity"));

        if (info.design_capacity != 0 && info.full_charged_capacity <= info.design_capacity)
            info.depreciation = 100 - (info.full_charged_capacity * 100 / info.design_capacity);

        const quint32 temperature = numberProperty(battery, CFSTR("Temperature")); // 0.01 degrees C
        if (temperature != 0)
            info.temperature = QString::number(temperature / 100.0);

        if (charging)
            info.state |= PowerOptions::Battery::CHARGING;
        else if (!external)
            info.state |= PowerOptions::Battery::DISCHARGING;
        if (external)
            info.state |= PowerOptions::Battery::POWER_ONLINE;
        if (result.battery_life_percent <= 5)
            info.state |= PowerOptions::Battery::CRITICAL;

        result.batteries.append(std::move(info));
        IOObjectRelease(battery);
    }

    return result;
}
