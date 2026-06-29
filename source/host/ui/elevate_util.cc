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
#include <QLibrary>
#include <QProcess>
#include <QtGui/qguiapplication_platform.h>
#endif // defined(Q_OS_LINUX)

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
        connect(watcher, &QWinEventNotifier::activated, this, [watcher, on_finished]()
        {
            watcher->deleteLater();
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
// otherwise reach the display. We reuse the connection Qt already opened for the xcb platform and
// resolve the entry points from the in-process libxcb - so there is no extra build or package
// dependency (nothing X11 is pulled onto headless servers), and this runs only in the GUI.
void setRootDisplayAccess(bool allow)
{
    auto* x11_app = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11_app)
        return;

    xcb_connection_t* connection = x11_app->connection();
    if (!connection)
        return;

    struct XcbVoidCookie { unsigned int sequence; };
    using ChangeHostsFn = XcbVoidCookie (*)(xcb_connection_t*, quint8, quint8, quint16, const quint8*);
    using FlushFn = int (*)(xcb_connection_t*);

    QLibrary xcb_library("xcb", 1);
    auto change_hosts = reinterpret_cast<ChangeHostsFn>(xcb_library.resolve("xcb_change_hosts"));
    auto flush = reinterpret_cast<FlushFn>(xcb_library.resolve("xcb_flush"));
    if (!change_hosts || !flush)
    {
        LOG(ERROR) << "Unable to resolve xcb entry points";
        return;
    }

    // X SECURITY: mode insert(0)/delete(1), family server-interpreted(5), address "type\0value".
    static const quint8 kRootAddress[] = "localuser\0root";
    change_hosts(connection, allow ? 0 : 1, 5, sizeof(kRootAddress) - 1, kRootAddress);
    flush(connection);
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
#else
    Q_UNUSED(parent);
    return nullptr;
#endif // defined(Q_OS_*)
}
