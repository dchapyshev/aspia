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

#include "host/service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>

#include "base/logging.h"
#include "base/security_log.h"
#include "host/database.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <aclapi.h>
#include "base/net/firewall_manager.h"
#include "base/win/security_helpers.h"
#include "host/host_storage.h"
#include "host/host_utils.h"
#include "host/win/safe_mode_util.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr char kFirewallTcpRuleName[] = "Aspia Host Service (TCP)";
constexpr char kFirewallUdpRuleName[] = "Aspia Host Service (UDP)";
constexpr char kFirewallRuleDecription[] = "Allow incoming TCP connections";

// Owner: SYSTEM. Protected DACL: SYSTEM and BUILTIN\Administrators have Full Control. Inherited
// by child containers and objects. Setting an explicit owner closes the implicit READ_CONTROL /
// WRITE_DAC rights that the previous owner (potentially a regular user who created the directory
// before the service started) would otherwise retain. Non-elevated administrator processes do
// not pass this DACL because in their tokens the Administrators SID is deny-only.
constexpr char kSecureFullDacl[] = "O:SYG:SYD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)";

// Like kSecureFullDacl, but Administrators get Generic Read only - cannot modify, delete or
// tamper with files. Used for the security log directory so its contents are read-only for
// admins viewing the log dialog and writable only by the service running as SYSTEM.
constexpr char kSecureReadOnlyDacl[] = "O:SYG:SYD:P(A;OICI;GA;;;SY)(A;OICI;GR;;;BA)";

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool applyPathSecurity(const QString& path, bool /* is_dir */, const char* sddl)
{
    ScopedSd sd = convertSddlToSd(sddl);
    if (!sd)
    {
        LOG(ERROR) << "convertSddlToSd failed";
        return false;
    }

    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    if (!GetSecurityDescriptorDacl(sd.get(), &present, &dacl, &defaulted) || !present)
    {
        PLOG(ERROR) << "GetSecurityDescriptorDacl failed";
        return false;
    }

    PSID owner = nullptr;
    if (!GetSecurityDescriptorOwner(sd.get(), &owner, &defaulted) || !owner)
    {
        PLOG(ERROR) << "GetSecurityDescriptorOwner failed";
        return false;
    }

    DWORD result = SetNamedSecurityInfoW(const_cast<wchar_t*>(qUtf16Printable(path)), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        owner, nullptr, dacl, nullptr);

    if (result != ERROR_SUCCESS)
    {
        LOG(ERROR) << "SetNamedSecurityInfoW failed for" << path << "error:" << result;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
// POSIX has no portable way to deny root access. Both DACLs collapse to the same chmod 0700/0600
// owned by root. Admin (root) keeps full access regardless; this is an OS-level limitation.
bool applyPathSecurity(const QString& path, bool is_dir, const char* /* sddl */)
{
    const QByteArray native = QFile::encodeName(path);

    // Reset the owner to root (uid 0, gid 0). If the entry was pre-created by a non-root user,
    // chmod alone is not enough - the user remains the owner and would retain access. Service
    // must run as root for this call to succeed.
    if (chown(native.constData(), 0, 0) != 0)
    {
        PLOG(ERROR) << "chown failed for" << path;
        return false;
    }

    const mode_t mode = is_dir ? S_IRWXU : (S_IRUSR | S_IWUSR);
    if (chmod(native.constData(), mode) != 0)
    {
        PLOG(ERROR) << "chmod failed for" << path;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_UNIX)

//--------------------------------------------------------------------------------------------------
// Creates the directory if needed and recursively applies the given DACL to it and all of its
// contents (files and subdirectories). Used at service startup to lock down storage shared by
// Database and SecurityLog. The SDDL string is ignored on POSIX.
bool applySecureDirectory(const QString& dir_path, const char* sddl)
{
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    if (!QDir().mkpath(dir_path))
    {
        LOG(ERROR) << "Unable to create directory:" << dir_path;
        return false;
    }

    bool ok = applyPathSecurity(dir_path, true, sddl);

    QDirIterator it(dir_path,
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QFileInfo entry(it.next());
        if (!applyPathSecurity(entry.absoluteFilePath(), entry.isDir(), sddl))
            ok = false;
    }

    return ok;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
const char Service::kFileName[] = "aspia_host.exe";
const char Service::kName[] = "aspia-host-service";
const char Service::kDisplayName[] = "Aspia Host Service";
const char Service::kDescription[] = "Accepts incoming remote desktop connections to this computer.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : CoreService(kName, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service is started";

    if (!applySecureDirectory(Database::directoryPath(), kSecureFullDacl))
        LOG(ERROR) << "Unable to apply secure permissions on database directory";

    if (!applySecureDirectory(securityLogDirectory(), kSecureReadOnlyDacl))
        LOG(ERROR) << "Unable to apply secure permissions on security log directory";

#if defined(Q_OS_WINDOWS)
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        PLOG(ERROR) << "SetPriorityClass failed";

    if (HostUtils::isMigrationNeeded())
        HostUtils::doMigrate();

    HostStorage storage;
    if (storage.isBootToSafeMode())
    {
        storage.setBootToSafeMode(false);

        if (!SafeModeUtil::setSafeMode(false))
            LOG(ERROR) << "Failed to turn off boot in safe mode";
        else
            LOG(INFO) << "Safe mode is disabled";

        if (!SafeModeUtil::setSafeModeService(kName, false))
            LOG(ERROR) << "Failed to remove service from boot in Safe Mode";
        else
            LOG(INFO) << "Service removed from safe mode loading";
    }
#endif // defined(Q_OS_WINDOWS)

    addFirewallRules();
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    deleteFirewallRules();
}

//--------------------------------------------------------------------------------------------------
void Service::addFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    if (!firewall.addTcpRule(kFirewallTcpRuleName, kFirewallRuleDecription))
    {
        LOG(ERROR) << "Unable to add firewall rule for TCP";
        return;
    }

    if (!firewall.addUdpRule(kFirewallUdpRuleName, kFirewallRuleDecription))
    {
        LOG(ERROR) << "Unable to add firewall rule for UDP";
        return;
    }

    LOG(INFO) << "Rules is added to the firewall";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::deleteFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    LOG(INFO) << "Delete firewall rules";
    firewall.deleteRuleByName(kFirewallTcpRuleName);
    firewall.deleteRuleByName(kFirewallUdpRuleName);
#endif // defined(Q_OS_WINDOWS)
}
