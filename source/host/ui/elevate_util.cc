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

#include "host/ui/elevate_util.h"

#include <QCoreApplication>
#include <QString>

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include <QDir>
#include <QWinEventNotifier>
#include <qt_windows.h>
#include <shellapi.h>

#include "base/process_util.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <unistd.h>

#include <QGuiApplication>
#include <QProcess>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include <unistd.h>

#include <QSocketNotifier>

#include <Security/Authorization.h>
#endif // defined(Q_OS_MACOS)

namespace {

#if defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
class WinElevateUtil final : public ElevateUtil
{
public:
    explicit WinElevateUtil(QObject* parent)
        : ElevateUtil(parent)
    {
        // Nothing.
    }

    // ElevateUtil implementation.
    bool runElevated(const QString& argument, quintptr parent_window,
                     std::function<void()> on_finished) final
    {
        if (ProcessUtil::isProcessElevated())
            return false;

        LOG(INFO) << "Process not elevated";

        QString exec_file = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        if (exec_file.isEmpty())
        {
            LOG(ERROR) << "Empty file path";
            return false;
        }

        SHELLEXECUTEINFOW sei;
        memset(&sei, 0, sizeof(sei));

        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = qUtf16Printable(exec_file);
        sei.hwnd = reinterpret_cast<HWND>(parent_window);
        sei.nShow = SW_SHOW;
        sei.lpParameters = qUtf16Printable(argument);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&sei))
        {
            PLOG(ERROR) << "ShellExecuteExW failed";
            return false;
        }

        QWinEventNotifier* watcher = new QWinEventNotifier(this);
        connect(watcher, &QWinEventNotifier::activated, this, [watcher, on_finished](HANDLE process)
        {
            watcher->deleteLater();
            // SEE_MASK_NOCLOSEPROCESS hands the process handle to the caller.
            CloseHandle(process);
            on_finished();
        });

        watcher->setHandle(sei.hProcess);
        watcher->setEnabled(true);
        return true;
    }
};

#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
// Grants or revokes access to the running X display for the root user via the X SECURITY extension
// (the same operation as "xhost +SI:localuser:root"). wlroots/labwc Xwayland authorizes clients by the
// session uid and issues no MIT cookie, so the dialog launched via pkexec (which runs as root) cannot
// otherwise reach the display. We reuse the connection Qt already opened for the xcb platform; the xcb
// entry points are linked statically into the binary, so there is no extra runtime dependency (nothing
// X11 is pulled onto headless servers). This runs only in the GUI.
void setRootDisplayAccess(bool allow)
{
    auto* x11_app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11_app)
        return;

    xcb_connection_t* connection = x11_app->connection();
    if (!connection)
        return;

    // X SECURITY: family server-interpreted, address "type\0value".
    static const quint8 kRootAddress[] = "localuser\0root";
    xcb_change_hosts(connection, allow ? XCB_HOST_MODE_INSERT : XCB_HOST_MODE_DELETE,
                     XCB_FAMILY_SERVER_INTERPRETED, sizeof(kRootAddress) - 1, kRootAddress);
    xcb_flush(connection);
}

//--------------------------------------------------------------------------------------------------
class LinuxElevateUtil final : public ElevateUtil
{
public:
    explicit LinuxElevateUtil(QObject* parent)
        : ElevateUtil(parent)
    {
        // Nothing.
    }

    // ElevateUtil implementation.
    bool runElevated(const QString& argument, quintptr /* parent_window */,
                     std::function<void()> on_finished) final
    {
        // Already root, or running setuid: edit the configuration in-process.
        if (getuid() == 0 || getuid() != geteuid())
            return false;

        LOG(INFO) << "Start dialog as super user";

        QProcess* process = new QProcess(this);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [process, on_finished](int exit_code, QProcess::ExitStatus exit_status)
        {
            LOG(INFO) << "Process finished with exit code:" << exit_code
                      << "(status:" << exit_status << ")";

            setRootDisplayAccess(false);
            process->deleteLater();
            on_finished();
        });

        // Run the application itself as root so its dialog can edit the system configuration. It is the
        // direct pkexec target, so the bundled polkit policy applies (branded prompt). pkexec preserves
        // DISPLAY/XAUTHORITY where the desktop issues an X cookie; on cookie-less Wayland compositors
        // (wlroots/labwc) setRootDisplayAccess grants the root helper access to the display instead.
        setRootDisplayAccess(true);
        process->start("pkexec", QStringList() << QCoreApplication::applicationFilePath() << argument);
        return true;
    }
};

#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)

//--------------------------------------------------------------------------------------------------
class MacElevateUtil final : public ElevateUtil
{
public:
    explicit MacElevateUtil(QObject* parent)
        : ElevateUtil(parent)
    {
        // Nothing.
    }

    // ElevateUtil implementation.
    bool runElevated(const QString& argument, quintptr /* parent_window */,
                     std::function<void()> on_finished) final
    {
        // Already root, or running setuid: edit the configuration in-process.
        if (getuid() == 0 || getuid() != geteuid())
            return false;

        LOG(INFO) << "Start dialog as super user";

        AuthorizationRef authorization = nullptr;
        OSStatus status = AuthorizationCreate(
            nullptr, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorization);
        if (status != errAuthorizationSuccess)
        {
            LOG(ERROR) << "AuthorizationCreate failed:" << status;
            return false;
        }

        // Run the application itself as root (for the root-owned config) but inside the console user's
        // Aqua session via "launchctl asuser <uid>" so its dialog reaches the WindowServer - a plain
        // privileged process runs outside the GUI session and cannot show UI. Requesting the right from
        // the application (not through an osascript helper) makes the system authentication prompt name
        // the host rather than the interpreter.
        QByteArray uid = QByteArray::number(getuid());
        QByteArray app = QCoreApplication::applicationFilePath().toLocal8Bit();
        QByteArray arg = argument.toLocal8Bit();
        char* arguments[] = { const_cast<char*>("asuser"), uid.data(), app.data(), arg.data(), nullptr };

        FILE* pipe = nullptr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // No non-deprecated replacement exists without shipping a signed SMJobBless helper.
        status = AuthorizationExecuteWithPrivileges(
            authorization, "/bin/launchctl", kAuthorizationFlagDefaults, arguments, &pipe);
#pragma clang diagnostic pop

        if (status != errAuthorizationSuccess)
        {
            if (status != errAuthorizationCanceled)
                LOG(ERROR) << "AuthorizationExecuteWithPrivileges failed:" << status;
            AuthorizationFree(authorization, kAuthorizationFlagDefaults);
            return false;
        }

        // The privileged tool's stdout is connected to |pipe|; it and the config dialog that inherits
        // the descriptor keep it open until the dialog closes. Watch for EOF to learn when it finished.
        QSocketNotifier* notifier = new QSocketNotifier(fileno(pipe), QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this,
                [notifier, pipe, authorization, on_finished]()
        {
            char buffer[256];
            if (::read(fileno(pipe), buffer, sizeof(buffer)) > 0)
                return; // Drain any output and keep waiting for EOF.

            notifier->setEnabled(false);
            notifier->deleteLater();
            fclose(pipe);
            AuthorizationFree(authorization, kAuthorizationFlagDefaults);
            on_finished();
        });

        return true;
    }
};

#endif // defined(Q_OS_MACOS)

} // namespace

//--------------------------------------------------------------------------------------------------
ElevateUtil::ElevateUtil(QObject* parent)
    : QObject(parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
ElevateUtil::~ElevateUtil() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ElevateUtil> ElevateUtil::create(QObject* parent)
{
#if defined(Q_OS_WINDOWS)
    return std::make_unique<WinElevateUtil>(parent);
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxElevateUtil>(parent);
#elif defined(Q_OS_MACOS)
    return std::make_unique<MacElevateUtil>(parent);
#else
    Q_UNUSED(parent);
    return nullptr;
#endif // defined(Q_OS_*)
}
