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
#include "host/workers/tools_worker.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QXmlStreamReader>
#include <qt_windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <UserEnv.h>
#include <WtsApi32.h>

#include <iterator>

#include "base/logging.h"
#include "base/win/file_version_info.h"
#include "base/win/registry.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/scoped_user_object.h"
#include "base/win/session_info.h"

namespace {

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
// Returns the Windows directory (for example "C:\Windows").
QString windowsDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };

    UINT length = GetWindowsDirectoryW(buffer, static_cast<UINT>(std::size(buffer)));
    if (!length || length >= std::size(buffer))
    {
        PLOG(ERROR) << "GetWindowsDirectoryW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer, length);
}

//--------------------------------------------------------------------------------------------------
// Returns the native system directory (for example "C:\Windows\System32"). A 32-bit service on a
// 64-bit system is redirected to SysWOW64, which has neither the management consoles nor the 64-bit
// tools; the "Sysnative" alias reaches the real System32 from a WOW64 process.
QString systemDirectory()
{
    BOOL is_wow64 = FALSE;
    if (IsWow64Process(GetCurrentProcess(), &is_wow64) && is_wow64)
    {
        QString windows_dir = windowsDirectory();
        if (windows_dir.isEmpty())
            return QString();

        return windows_dir + "\\Sysnative";
    }

    wchar_t buffer[MAX_PATH] = { 0 };

    UINT length = GetSystemDirectoryW(buffer, static_cast<UINT>(std::size(buffer)));
    if (!length || length >= std::size(buffer))
    {
        PLOG(ERROR) << "GetSystemDirectoryW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer, length);
}

//--------------------------------------------------------------------------------------------------
// Returns the shell the scripts are written for.
QString scriptInterpreter(const QString& system_dir)
{
    return system_dir + "\\WindowsPowerShell\\v1.0\\powershell.exe";
}

//--------------------------------------------------------------------------------------------------
// Returns the description the operating system itself gives the file (localized), or an empty string
// if the file has no version resource (management consoles do not have one).
QString fileDescription(const QString& file_path)
{
    std::unique_ptr<FileVersionInfo> version_info = FileVersionInfo::createFileVersionInfo(file_path);
    if (!version_info)
        return QString();

    return version_info->fileDescription();
}

//--------------------------------------------------------------------------------------------------
// Returns the file that IS the tool, the one the system can name and draw an icon for. control.exe,
// mmc.exe and explorer.exe merely host whatever is passed to them, so for them it is the argument;
// an argument that is not a file at all (a settings page) leaves the tool without such a file.
QString toolFile(const QString& program, const QStringList& arguments)
{
    if (arguments.isEmpty())
        return program;

    if (QFileInfo::exists(arguments.first()))
        return arguments.first();

    return QString();
}

//--------------------------------------------------------------------------------------------------
// Encodes the icon as PNG.
QByteArray encodeIcon(HICON icon)
{
    const QImage image = QImage::fromHICON(icon);
    if (image.isNull())
    {
        LOG(WARNING) << "Unable to convert icon";
        return QByteArray();
    }

    QByteArray buffer;
    QBuffer device(&buffer);

    if (!device.open(QIODevice::WriteOnly) || !image.save(&device, "PNG"))
    {
        LOG(WARNING) << "Unable to encode icon";
        return QByteArray();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
// Returns the icon the local user sees for the file in the system menus.
QByteArray shellIcon(const QString& file_path)
{
    SHFILEINFOW file_info;
    memset(&file_info, 0, sizeof(file_info));

    SHGetFileInfoW(qUtf16Printable(file_path), 0, &file_info, sizeof(file_info),
                   SHGFI_ICON | SHGFI_LARGEICON);

    ScopedHICON icon(file_info.hIcon);
    if (!icon.isValid())
    {
        LOG(WARNING) << "Unable to get icon for" << file_path;
        return QByteArray();
    }

    return encodeIcon(icon);
}

//--------------------------------------------------------------------------------------------------
// Returns the first icon stored in the resources of the library or the executable.
QByteArray resourceIcon(const QString& file_path)
{
    if (file_path.isEmpty())
        return QByteArray();

    HICON icon_handle = nullptr;
    if (ExtractIconExW(qUtf16Printable(file_path), 0, &icon_handle, nullptr, 1) != 1)
    {
        LOG(WARNING) << "No icons in" << file_path;
        return QByteArray();
    }

    ScopedHICON icon(icon_handle);
    return encodeIcon(icon);
}

//--------------------------------------------------------------------------------------------------
// Looks up the snap-in registered under |clsid| and returns its name and the library its resources
// live in. Both are empty for anything that is not a snap-in and for the folder nodes of the console
// itself.
QPair<QString, QString> snapInByClsid(const QString& clsid)
{
    RegKey key;
    if (key.open(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\MMC\\SnapIns\\" + clsid,
                 KEY_READ) != ERROR_SUCCESS)
    {
        return QPair<QString, QString>();
    }

    // The library implementing the snap-in. The console implements its own folder nodes, and those
    // are listed in a console file just like the real snap-in - skip them.
    QString server_file;

    RegKey server_key;
    if (server_key.open(HKEY_CLASSES_ROOT, "CLSID\\" + clsid + "\\InprocServer32",
                        KEY_READ) == ERROR_SUCCESS)
    {
        server_key.readValue(QString(), &server_file);

        if (server_file.endsWith("mmcndmgr.dll", Qt::CaseInsensitive))
            return QPair<QString, QString>();
    }

    QString name;
    QString resource_file;

    // "@some.dll,-100": the localized name and the icon of the snap-in live in that library.
    QString indirect_name;
    if (key.readValue("NameStringIndirect", &indirect_name) == ERROR_SUCCESS &&
        indirect_name.startsWith('@'))
    {
        wchar_t buffer[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHLoadIndirectString(qUtf16Printable(indirect_name), buffer,
                                            static_cast<UINT>(std::size(buffer)), nullptr)))
        {
            name = QString::fromWCharArray(buffer);
        }

        resource_file = indirect_name.mid(1, indirect_name.lastIndexOf(',') - 1);
    }

    if (name.isEmpty())
        key.readValue("NameString", &name);

    if (resource_file.isEmpty())
        resource_file = server_file;

    return QPair<QString, QString>(name, resource_file);
}

//--------------------------------------------------------------------------------------------------
// Returns the name of the snap-in hosted by the console file and the library its resources live in:
// a .msc has neither a name nor an icon of its own. It is an XML document whose scope tree lists the
// nodes of the console by the CLSID of the snap-in behind them; the first one that is a registered
// snap-in is the tool itself, the rest are its extensions.
QPair<QString, QString> readSnapIn(const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(WARNING) << "Unable to open" << file_path;
        return QPair<QString, QString>();
    }

    QXmlStreamReader xml(&file);

    while (!xml.atEnd())
    {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QString("Node"))
            continue;

        const QString clsid = xml.attributes().value("CLSID").toString();
        if (clsid.isEmpty())
            continue;

        QPair<QString, QString> snap_in = snapInByClsid(clsid);
        if (!snap_in.second.isEmpty())
            return snap_in;
    }

    LOG(WARNING) << "No snap-in found in" << file_path;
    return QPair<QString, QString>();
}

//--------------------------------------------------------------------------------------------------
// Returns the profile directory of the user logged on to |session_id|. A script started with the
// token of the service gets the environment of the service, so the paths of the user it may need are
// passed to it separately.
QString userProfileDirectory(SessionId session_id)
{
    ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(ERROR) << "WTSQueryUserToken failed";
        return QString();
    }

    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD length = static_cast<DWORD>(std::size(buffer));

    if (!GetUserProfileDirectoryW(user_token, buffer, &length))
    {
        PLOG(ERROR) << "GetUserProfileDirectoryW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | desired_access,
                          process_token.recieve()))
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
// Returns a copy of the current process token with the SE_TCB_NAME privilege enabled. Changing the
// session of a token requires it.
bool createPrivilegedToken(ScopedHandle* token_out)
{
    ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
// Creates the token to start a tool with: always the token of the user logged on to |session_id|,
// never the token of the service. An administrative tool needs the highest privileges available to
// that user, so for an administrator (whose token UAC splits in two) the elevated token linked to
// the filtered one is taken; a user without administrative rights simply gets their own token.
//
// An elevated token is additionally marked as having UI access: a process created by the service can
// not take the foreground away from the windows already open in the session (the foreground lock), and
// a process with UI access is exempt from that lock, so the tool appears in front instead of behind
// everything. The mark goes on an elevated token only. Every process started from the token inherits
// it, and on the token of an ordinary user it would leave that user with medium-integrity processes
// exempt from the isolation of the user interface - a way around that isolation rather than a way to
// the front.
bool createToken(SessionId session_id, bool elevated, ScopedHandle* token_out)
{
    ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    ScopedHandle token;

    if (elevated)
    {
        TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
        DWORD length = 0;

        if (!GetTokenInformation(user_token, TokenElevationType, &elevation_type,
                                 sizeof(elevation_type), &length))
        {
            PLOG(ERROR) << "GetTokenInformation failed";
        }
        else if (elevation_type == TokenElevationTypeLimited)
        {
            TOKEN_LINKED_TOKEN linked_token;
            memset(&linked_token, 0, sizeof(linked_token));

            if (!GetTokenInformation(user_token, TokenLinkedToken, &linked_token,
                                     sizeof(linked_token), &length))
            {
                PLOG(ERROR) << "GetTokenInformation failed";
            }
            else
            {
                ScopedHandle linked(linked_token.LinkedToken);

                // The linked token is an impersonation one; a new process needs a primary token.
                if (!DuplicateTokenEx(linked, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation,
                                      TokenPrimary, token.recieve()))
                {
                    PLOG(ERROR) << "DuplicateTokenEx failed";
                }
            }
        }
    }

    const bool is_elevated = token.isValid();

    // The user is not an administrator, UAC is disabled or the linked token is unavailable: their
    // own token already carries everything they have.
    if (!token.isValid())
        token.reset(user_token.release());

    if (is_elevated)
    {
        ScopedHandle privileged_token;
        if (!createPrivilegedToken(&privileged_token))
        {
            LOG(ERROR) << "createPrivilegedToken failed";
            return false;
        }

        ScopedImpersonator impersonator;
        if (!impersonator.loggedOnUser(privileged_token))
        {
            LOG(ERROR) << "Failed to impersonate thread";
            return false;
        }

        DWORD enabled = 1;
        if (!SetTokenInformation(token, TokenUIAccess, &enabled, sizeof(enabled)))
        {
            PLOG(ERROR) << "SetTokenInformation failed";
            return false;
        }
    }

    token_out->reset(token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
// Creates the token to start a script with: the token of the service itself, moved into
// |session_id|. A script does what the person at the machine is usually not allowed to do (stopping
// services, touching the files of the system), so the rights of the logged on user are not enough;
// the session is changed so that the console window of the script appears on the desktop the client
// sees, instead of the invisible one the service lives on.
bool createServiceToken(SessionId session_id, ScopedHandle* token_out)
{
    ScopedHandle token;
    if (!copyProcessToken(TOKEN_ALL_ACCESS, &token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    // Both changing the session of a token and marking it as having UI access require the privilege.
    ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(ERROR) << "Failed to impersonate thread";
        return false;
    }

    DWORD target_session = session_id;
    if (!SetTokenInformation(token, TokenSessionId, &target_session, sizeof(target_session)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    token_out->reset(token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool createProcessWithToken(HANDLE token, const QString& command_line)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_SHOWNORMAL;

    void* environment = nullptr;
    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    // The service has no console of its own: a console tool needs one created for it.
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;

    const BOOL result = CreateProcessAsUserW(
        token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)), nullptr, nullptr,
        FALSE, flags, environment, nullptr, &startup_info, &process_info);

    DestroyEnvironmentBlock(environment);

    if (!result)
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        return false;
    }

    ScopedHandle thread_handle(process_info.hThread);
    ScopedHandle process_handle(process_info.hProcess);

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void ToolsWorker::buildToolList()
{
    const QString system_dir = systemDirectory();
    const QString windows_dir = windowsDirectory();

    if (system_dir.isEmpty() || windows_dir.isEmpty())
    {
        LOG(ERROR) << "Unable to determine system directories";
        return;
    }

    // Adds the tool if its executable file is present on this machine. Everything offered to the
    // client is known to exist: the menu on the client side has no disabled items.
    // |resource_file| is needed only when the tool itself is not a file: a settings page opened
    // through the shell has its name and icon in the application behind it.
    auto add = [this](const QString& name, const QString& program, const QStringList& arguments,
                      bool elevated, const QString& resource_file = QString())
    {
        if (!QFileInfo::exists(program))
        {
            LOG(INFO) << "Tool" << name << "is not available (" << program << ")";
            return;
        }

        const QString tool_file =
            resource_file.isEmpty() ? toolFile(program, arguments) : resource_file;

        Tool tool;
        tool.name = name;

        if (tool_file.endsWith(".msc", Qt::CaseInsensitive))
        {
            const QPair<QString, QString> snap_in = readSnapIn(tool_file);

            if (!snap_in.first.isEmpty())
                tool.name = snap_in.first;

            tool.icon = resourceIcon(snap_in.second);
        }
        else if (!tool_file.isEmpty())
        {
            const QString description = fileDescription(tool_file);
            if (!description.isEmpty())
                tool.name = description;

            tool.icon = shellIcon(tool_file);
        }

        tool.program = QDir::toNativeSeparators(program);
        tool.arguments = arguments;
        tool.elevated = elevated;

        tools_.append(tool);
    };

    const QString mmc = system_dir + "\\mmc.exe";
    const QString control = system_dir + "\\control.exe";
    const QString explorer = windows_dir + "\\explorer.exe";

    // Adds a management console snap-in. The console itself hosts the snap-in, so both files must be
    // present. A missing .msc is the normal way an edition without the snap-in (gpedit.msc on
    // Windows Home, for example) reports its absence.
    auto add_snap_in = [&](const QString& name, const QString& snap_in_name)
    {
        const QString snap_in = system_dir + '\\' + snap_in_name;

        if (!QFileInfo::exists(snap_in))
        {
            LOG(INFO) << "Tool" << name << "is not available (" << snap_in << ")";
            return;
        }

        // The console reads the path itself, and "Sysnative" is an alias that exists only inside the
        // 32-bit process looking through it, so what is passed on is the directory as everyone else
        // on the machine knows it.
        add(name, mmc, QStringList() << QDir::toNativeSeparators(
            windows_dir + "\\System32\\" + snap_in_name), true);
    };

    add("Task Manager", system_dir + "\\taskmgr.exe", QStringList(), true);
    add("Resource Monitor", system_dir + "\\resmon.exe", QStringList(), true);
    add("System Configuration", system_dir + "\\msconfig.exe", QStringList(), true);
    add("System Information", system_dir + "\\msinfo32.exe", QStringList(), true);
    add("Disk Cleanup", system_dir + "\\cleanmgr.exe", QStringList(), true);
    add("Defragment and Optimize Drives", system_dir + "\\dfrgui.exe", QStringList(), true);

    add("Registry Editor", windows_dir + "\\regedit.exe", QStringList(), true);

    // Computer Management hosts the device manager, the disk management, the services, the event
    // viewer, the task scheduler, the shared folders, the local users and the performance monitor,
    // so those consoles are not offered on their own.
    add_snap_in("Computer Management", "compmgmt.msc");
    add_snap_in("Local Group Policy Editor", "gpedit.msc");
    add_snap_in("Certificates", "certlm.msc");
    add_snap_in("Print Management", "printmanagement.msc");

    add_snap_in("Firewall with Advanced Security", "wf.msc");

    // The shell tools are started as the logged on user: the file manager and the modern settings
    // application are per-user applications and behave incorrectly under the system account.
    add("Control Panel", control, QStringList(), false);

    // The settings application is opened through the shell by its URI, so its own executable is
    // where the name and the icon come from. Its absence means the system has no such application.
    const QString settings_app = windows_dir + "\\ImmersiveControlPanel\\SystemSettings.exe";
    if (QFileInfo::exists(settings_app))
        add("Settings", explorer, QStringList() << "ms-settings:", false, settings_app);

    add("File Explorer", explorer, QStringList(), false);
    add("Command Prompt", system_dir + "\\cmd.exe", QStringList(), true);
    add("Windows PowerShell", system_dir + "\\WindowsPowerShell\\v1.0\\powershell.exe",
        QStringList(), true);
}

//--------------------------------------------------------------------------------------------------
// static
std::span<const ToolsWorker::Script> ToolsWorker::scriptTable()
{
    static const Script kScripts[] =
    {
        // The reports come first: they are what tells the operator where to look before touching
        // anything.
        { "Show Event Log Errors",
          "Lists what the system and the applications reported as a failure in the last 24 hours",
          "win/show_event_log_errors.ps1", false },

        { "Show Unexpected Shutdowns",
          "Lists the crashes and unexpected restarts of the last 30 days, and who asked for the "
          "planned ones",
          "win/show_unexpected_shutdowns.ps1", false },

        { "Show Stopped Automatic Services",
          "Lists the services set to start with the system that are not running",
          "win/show_stopped_automatic_services.ps1", false },

        { "Flush DNS, ARP and NetBIOS Caches",
          "Clears the name and address caches of the network stack",
          "win/flush_dns_arp_and_netbios_caches.ps1", false },

        { "Synchronize System Time",
          "Resynchronizes the clock of this computer with its time source",
          "win/synchronize_system_time.ps1", false },

        { "Restart Print Spooler",
          "Restarts the printing service and removes everything queued for printing",
          "win/restart_print_spooler.ps1", true },

        // The shell belongs to the user, so this one runs as the user: a shell started with the
        // token of the service would own the desktop of the session as the system account.
        { "Restart Windows Explorer",
          "Restarts the desktop shell of the logged on user",
          "win/restart_windows_explorer.ps1", true, true },

        { "Create Restore Point",
          "Creates a system restore point of this computer",
          "win/create_restore_point.ps1", false },

        { "Update Defender and Run Quick Scan",
          "Updates the antivirus signatures and checks the places malicious software starts from",
          "win/update_defender_and_run_quick_scan.ps1", false },

        { "Free Up Disk Space",
          "Removes the temporary files of the logged on user and of the system and empties the "
          "recycle bin of that user",
          "win/free_up_disk_space.ps1", true },
    };

    return kScripts;
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::scriptsSupported() const
{
    // Everything is written for the shell of the system; it is a part of Windows, but a machine
    // without it is offered nothing rather than items that fail on the first line.
    const QString system_dir = systemDirectory();
    if (system_dir.isEmpty() || !QFileInfo::exists(scriptInterpreter(system_dir)))
    {
        LOG(ERROR) << "Windows PowerShell is not available; no scripts are offered";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::hasInteractiveUser(SessionId session_id) const
{
    if (session_id == kInvalidSessionId || session_id == kServiceSessionId)
        return false;

    SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(WARNING) << "Unable to query session info (sid" << session_id << ")";
        return false;
    }

    // At the logon screen the session exists, but has no user: the tool would be started on the
    // default desktop, which nobody sees while the Winlogon desktop is active.
    return !session_info.userName().isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchTool(SessionId session_id, const Tool& tool) const
{
    // The administrative tools are started with the elevated token of the logged on user, so they
    // work without asking them for elevation.
    ScopedHandle token;
    if (!createToken(session_id, tool.elevated, &token))
    {
        LOG(ERROR) << "createToken failed (sid" << session_id << ")";
        return false;
    }

    QString command_line = '"' + tool.program + '"';
    for (const QString& argument : tool.arguments)
        command_line += " \"" + argument + '"';

    LOG(INFO) << "Starting tool" << tool.name << "in session" << session_id
              << "(elevated:" << tool.elevated << "cmd:" << command_line << ")";

    return createProcessWithToken(token, command_line);
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchScript(SessionId session_id, const Script& script) const
{
    const QString system_dir = systemDirectory();
    if (system_dir.isEmpty())
    {
        LOG(ERROR) << "Unable to determine system directory";
        return false;
    }

    // A value the shell reads as text is written between apostrophes, and an apostrophe inside such
    // a value is written twice.
    QString title = script.name;
    title.replace('\'', "''");

    QString user_profile = userProfileDirectory(session_id);
    user_profile.replace('\'', "''");

    const QString body = scriptText(script);
    if (body.isEmpty())
        return false;

    // The body is given what it can not find out on its own: started with the token of the service,
    // it sees the environment of the service and not the one of the user it acts for. It is wrapped
    // so that the window survives what happens inside it - an error is printed instead of closing
    // the window in the face of the person reading it, and the window stays until a key is pressed.
    // A key, not a command prompt: an interactive shell left on the desktop with these privileges is
    // a gift to whoever walks up to the machine.
    const QString text = QString(
        "$Host.UI.RawUI.WindowTitle = 'Aspia - %1'\n"
        "$ProgressPreference = 'SilentlyContinue'\n"
        "$ErrorActionPreference = 'Stop'\n"
        "$SessionId = %2\n"
        "$UserProfile = '%3'\n"
        "try\n"
        "{\n"
        "%4"
        "}\n"
        "catch\n"
        "{\n"
        "    Write-Host $_.Exception.Message -ForegroundColor Red\n"
        "}\n"
        "Write-Host ''\n"
        "Write-Host 'Press any key to close this window...'\n"
        "$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')\n")
        .arg(title).arg(session_id).arg(user_profile, body);

    // The script is handed over as base64 of its text: a script of several lines does not have to
    // survive the quoting rules of a command line, and the execution policy has nothing to say about
    // it (it applies to script files, not to what is given on the command line).
    const QByteArray encoded = QByteArray(
        reinterpret_cast<const char*>(text.utf16()), text.size() * 2).toBase64();

    const QString command_line = '"' + QDir::toNativeSeparators(scriptInterpreter(system_dir)) +
        "\" -NoProfile -EncodedCommand " + QString::fromLatin1(encoded);

    ScopedHandle token;

    if (script.as_user)
    {
        if (!createToken(session_id, /* elevated */ false, &token))
        {
            LOG(ERROR) << "createToken failed (sid" << session_id << ")";
            return false;
        }
    }
    else if (!createServiceToken(session_id, &token))
    {
        LOG(ERROR) << "createServiceToken failed (sid" << session_id << ")";
        return false;
    }

    LOG(INFO) << "Starting script" << script.name << "in session" << session_id
              << "(as_user:" << script.as_user << ")";

    return createProcessWithToken(token, command_line);
}
