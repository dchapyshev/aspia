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

#include "base/power_controller.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <WtsApi32.h>

#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/session_info.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#elif defined(Q_OS_MACOS)
#include <QProcess>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/stat.h>
#endif // defined(Q_OS_WINDOWS)

namespace {

#if defined(Q_OS_WINDOWS)
// Delay for shutdown and reboot.
const DWORD kActionDelayInSeconds = 0;

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | desired_access, process_token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token, desired_access, nullptr, SecurityImpersonation,
        TokenPrimary, token_out->recieve()))
    {
        PLOG(ERROR) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// Creates a copy of the current process with SE_SHUTDOWN_NAME privilege enabled.
bool createPrivilegedToken(ScopedHandle* token_out)
{
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    ScopedHandle privileged_token;
    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    // Get the LUID for the SE_SHUTDOWN_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &state.Privileges[0].Luid))
    {
        PLOG(ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_SHUTDOWN_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
//--------------------------------------------------------------------------------------------------
QDBusMessage logindManagerCall(const QString& method)
{
    return QDBusMessage::createMethodCall(
        "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", method);
}

//--------------------------------------------------------------------------------------------------
bool callLogind(const QDBusMessage& call)
{
    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage)
    {
        LOG(ERROR) << "logind call" << call.member() << "failed:" << reply.errorMessage();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// Returns the id of the active session on the primary seat, or an empty string if there is none.
QString activeSessionId()
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.login1", "/org/freedesktop/login1/seat/seat0",
        "org.freedesktop.DBus.Properties", "Get");
    call << QString("org.freedesktop.login1.Seat") << QString("ActiveSession");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return QString();

    // The ActiveSession property is a (so) structure: session id and object path.
    const QDBusArgument arg =
        reply.arguments().constFirst().value<QDBusVariant>().variant().value<QDBusArgument>();

    QString session_id;
    QDBusObjectPath object_path;

    arg.beginStructure();
    arg >> session_id >> object_path;
    arg.endStructure();

    return session_id;
}
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_MACOS)
//--------------------------------------------------------------------------------------------------
// Triggers a graceful power action the way the Apple menu does: sends |loginwindow_event| (a raw Apple
// event such as "«event aevtshut»") to loginwindow in the console user's GUI session, which lets apps
// terminate cleanly. Invoked from the root service, so it hops into that session via
// "launchctl asuser <uid> sudo -u <user>". Falls back to shutdown(8) (|fallback_args|) when there is
// no GUI console user (e.g. the login window) or the event could not be delivered.
bool macPowerAction(const QString& loginwindow_event, const QStringList& fallback_args)
{
    struct stat console_stat;
    if (stat("/dev/console", &console_stat) == 0 && console_stat.st_uid != 0)
    {
        passwd* pw = getpwuid(console_stat.st_uid);
        if (pw && pw->pw_name)
        {
            const QString script =
                QString::fromUtf8("tell application \"loginwindow\" to ") + loginwindow_event;

            const int rc = QProcess::execute("launchctl",
                { "asuser", QString::number(console_stat.st_uid),
                  "sudo", "-u", QString::fromUtf8(pw->pw_name), "osascript", "-e", script });
            if (rc == 0)
                return true;
        }
    }

    return QProcess::execute("/sbin/shutdown", fallback_args) == 0;
}
#endif // defined(Q_OS_MACOS)

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool PowerController::shutdown()
{
#if defined(Q_OS_WINDOWS)
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    ScopedHandle process_token;
    if (!copyProcessToken(desired_access, &process_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(ERROR) << "loggedOnUser failed";
        return false;
    }

    const DWORD reason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE;

    bool result = !!InitiateSystemShutdownExW(nullptr, nullptr, kActionDelayInSeconds, TRUE, FALSE, reason);
    if (!result)
    {
        PLOG(ERROR) << "InitiateSystemShutdownExW failed";
    }

    return result;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDBusMessage call = logindManagerCall("PowerOff");
    call << false;
    return callLogind(call);
#elif defined(Q_OS_MACOS)
    // Graceful shut down (Apple menu "Shut Down"), falling back to shutdown(8) with no GUI user.
    return macPowerAction(QString::fromUtf8("«event aevtshut»"), { "-h", "now" });
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool PowerController::reboot()
{
#if defined(Q_OS_WINDOWS)
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    ScopedHandle process_token;
    if (!copyProcessToken(desired_access, &process_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(ERROR) << "loggedOnUser failed";
        return false;
    }

    const DWORD reason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE;

    bool result = !!InitiateSystemShutdownExW(nullptr, nullptr, kActionDelayInSeconds, TRUE, TRUE, reason);
    if (!result)
        PLOG(ERROR) << "InitiateSystemShutdownExW failed";

    return result;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QDBusMessage call = logindManagerCall("Reboot");
    call << false;
    return callLogind(call);
#elif defined(Q_OS_MACOS)
    // Graceful restart (Apple menu "Restart"), falling back to shutdown(8) with no GUI user.
    return macPowerAction(QString::fromUtf8("«event aevtrrst»"), { "-r", "now" });
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool PowerController::logoff()
{
#if defined(Q_OS_WINDOWS)
    DWORD session_id = kInvalidSessionId;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
        PLOG(ERROR) << "ProcessIdToSessionId failed";

    if (session_id != kInvalidSessionId)
    {
        SessionInfo session_info(session_id);
        if (session_info.connectState() != SessionInfo::ConnectState::ACTIVE)
        {
            LOG(INFO) << "User session not in active state. Logoff not required";
            return true;
        }
    }

    if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, session_id, FALSE))
    {
        PLOG(ERROR) << "WTSLogoffSession failed";
        return false;
    }

    return true;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    QString session_id = activeSessionId();
    if (session_id.isEmpty())
    {
        LOG(ERROR) << "No active session to log off";
        return false;
    }

    QDBusMessage call = logindManagerCall("TerminateSession");
    call << session_id;
    return callLogind(call);
#elif defined(Q_OS_MACOS)
    // Called from the user's Aqua session (the desktop agent). Send the "log out" Apple event
    // (kAELogOut) to loginwindow, which logs out immediately without the confirmation dialog. The raw
    // event code requires guillemets, which is why the script is decoded from UTF-8.
    const QString script =
        QString::fromUtf8("tell application \"loginwindow\" to «event aevtrlgo»");
    return QProcess::execute("osascript", { "-e", script }) == 0;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool PowerController::lock()
{
#if defined(Q_OS_WINDOWS)
    if (!LockWorkStation())
    {
        PLOG(ERROR) << "LockWorkStation failed";
        return false;
    }

    return true;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    return callLogind(logindManagerCall("LockSessions"));
#elif defined(Q_OS_MACOS)
    // Called from the user's Aqua session (the desktop agent). SACLockScreenImmediate (private
    // login.framework) is the reliable programmatic lock on modern macOS. The framework is
    // Apple-signed, so dlopen works under the hardened runtime.
    void* handle = dlopen(
        "/System/Library/PrivateFrameworks/login.framework/Versions/Current/login", RTLD_LAZY);
    if (!handle)
    {
        LOG(ERROR) << "dlopen login.framework failed:" << dlerror();
        return false;
    }

    using LockScreenFn = void (*)(void);
    auto lock_screen = reinterpret_cast<LockScreenFn>(dlsym(handle, "SACLockScreenImmediate"));
    if (!lock_screen)
    {
        LOG(ERROR) << "SACLockScreenImmediate not found";
        dlclose(handle);
        return false;
    }

    lock_screen();
    dlclose(handle);
    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}
