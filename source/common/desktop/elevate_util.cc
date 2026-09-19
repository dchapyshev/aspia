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

#include "common/desktop/elevate_util.h"

#include <QCoreApplication>
#include <QStringList>

#include "base/logging.h"
#include "base/process_util.h"

#if defined(Q_OS_WINDOWS)
#include <QDir>
#include <QWinEventNotifier>
#include <qt_windows.h>
#include <shellapi.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <unistd.h>
#include <QGuiApplication>
#include <QProcess>
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_MACOS)
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <QSocketNotifier>
#include <Security/Authorization.h>
#endif // defined(Q_OS_MACOS)

namespace {

#if defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
// Quotes an argument the way the C runtime of the new process expects to find it. Windows passes the
// command line as a single string and every process splits it on its own.
QString quoteArgument(const QString& argument)
{
    if (!argument.isEmpty() && !argument.contains(' ') && !argument.contains('\t') &&
        !argument.contains('"'))
    {
        return argument;
    }

    QString result('"');

    for (qsizetype i = 0; i < argument.size(); ++i)
    {
        qsizetype backslashes = 0;
        while (i < argument.size() && argument[i] == '\\')
        {
            ++backslashes;
            ++i;
        }

        if (i == argument.size())
        {
            // The backslashes in front of the closing quote are doubled, so that they do not turn
            // it into a quote of their own.
            result += QString(backslashes * 2, '\\');
            break;
        }

        if (argument[i] == '"')
            result += QString(backslashes * 2 + 1, '\\');
        else
            result += QString(backslashes, '\\');

        result += argument[i];
    }

    result += '"';
    return result;
}

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
    bool runElevated(const QStringList& arguments, quintptr parent_window,
                     std::function<void(int)> on_finished) final
    {
        if (ProcessUtil::isPrivileged())
            return false;

        LOG(INFO) << "Process not elevated";

        QString exec_file = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        if (exec_file.isEmpty())
        {
            LOG(ERROR) << "Empty file path";
            return false;
        }

        // Both strings have to outlive the call, so they are taken from objects of our own and not
        // from temporaries.
        QString parameters;
        for (const QString& argument : arguments)
        {
            if (!parameters.isEmpty())
                parameters += ' ';

            parameters += quoteArgument(argument);
        }

        SHELLEXECUTEINFOW sei;
        memset(&sei, 0, sizeof(sei));

        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<const wchar_t*>(exec_file.utf16());
        sei.hwnd = reinterpret_cast<HWND>(parent_window);
        sei.nShow = SW_SHOW;
        sei.lpParameters = reinterpret_cast<const wchar_t*>(parameters.utf16());
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&sei))
        {
            PLOG(ERROR) << "ShellExecuteExW failed";
            return false;
        }

        QWinEventNotifier* watcher = new QWinEventNotifier(this);
        connect(watcher, &QWinEventNotifier::activated, this, [watcher, on_finished](HANDLE process)
        {
            watcher->setEnabled(false);
            watcher->deleteLater();

            DWORD exit_code = 0;
            if (!GetExitCodeProcess(process, &exit_code))
            {
                PLOG(ERROR) << "GetExitCodeProcess failed";
                exit_code = static_cast<DWORD>(kNoExitCode);
            }

            // SEE_MASK_NOCLOSEPROCESS hands the process handle to the caller.
            CloseHandle(process);
            on_finished(static_cast<int>(exit_code));
        });

        watcher->setHandle(sei.hProcess);
        watcher->setEnabled(true);
        return true;
    }
};

#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

// What pkexec returns when the user dismissed the authentication dialog. Everything else it
// returns for itself, 127 among it, is a failure to report.
const int kPkexecDismissed = 126;

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
    bool runElevated(const QStringList& arguments, quintptr /* parent_window */,
                     std::function<void(int)> on_finished) final
    {
        // Already root, or running setuid: do the work in-process.
        if (ProcessUtil::isPrivileged())
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

            if (exit_code == kPkexecDismissed)
            {
                // pkexec never started the application, this code is its own.
                on_finished(kDeclinedExitCode);
            }
            else if (exit_status != QProcess::NormalExit)
            {
                // A process killed by a signal reports the signal instead of a code of its own.
                on_finished(kNoExitCode);
            }
            else
            {
                on_finished(exit_code);
            }
        });

        // A process that failed to start reports no exit code and emits nothing else, so the caller
        // hears about it here or waits forever.
        connect(process, &QProcess::errorOccurred, this,
                [process, on_finished](QProcess::ProcessError error)
        {
            if (error != QProcess::FailedToStart)
                return;

            LOG(ERROR) << "Unable to start process:" << process->errorString();

            setRootDisplayAccess(false);
            process->deleteLater();
            on_finished(kNoExitCode);
        });

        // Run the application itself as root so its dialog can edit the system configuration. It is the
        // direct pkexec target, so the bundled polkit policy applies (branded prompt). pkexec preserves
        // DISPLAY/XAUTHORITY where the desktop issues an X cookie; on cookie-less Wayland compositors
        // (wlroots/labwc) setRootDisplayAccess grants the root helper access to the display instead.
        setRootDisplayAccess(true);
        process->start("pkexec", QStringList() << QCoreApplication::applicationFilePath() << arguments);
        return true;
    }
};

#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_MACOS)

// What the started process writes into the pipe to say how it ended.
const char kExitCodeMark[] = "aspia exit code ";

//--------------------------------------------------------------------------------------------------
int exitCodeFromOutput(const QByteArray& output)
{
    qsizetype position = output.lastIndexOf(kExitCodeMark);
    if (position < 0)
        return ElevateUtil::kNoExitCode;

    QByteArray tail = output.mid(position + static_cast<qsizetype>(qstrlen(kExitCodeMark)));
    qsizetype end = tail.indexOf('\n');
    if (end >= 0)
        tail.truncate(end);

    bool converted = false;
    int exit_code = tail.trimmed().toInt(&converted);

    return converted ? exit_code : ElevateUtil::kNoExitCode;
}

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
    bool runElevated(const QStringList& arguments, quintptr /* parent_window */,
                     std::function<void(int)> on_finished) final
    {
        // Already root, or running setuid: do the work in-process.
        if (ProcessUtil::isPrivileged())
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

        // A privileged process runs outside the GUI session and cannot show windows, so the
        // application is started by "launchctl asuser" inside the session of the console user.
        // Asking for the right from the application makes the system prompt name the host and not
        // an interpreter.
        QList<QByteArray> argument_data;
        argument_data.append("asuser");
        argument_data.append(QByteArray::number(getuid()));
        argument_data.append(QCoreApplication::applicationFilePath().toLocal8Bit());

        for (const QString& argument : arguments)
            argument_data.append(argument.toLocal8Bit());

        std::vector<char*> argument_pointers;

        for (QByteArray& argument : argument_data)
            argument_pointers.push_back(argument.data());

        argument_pointers.push_back(nullptr);

        FILE* pipe = nullptr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // No non-deprecated replacement exists without shipping a signed SMJobBless helper.
        status = AuthorizationExecuteWithPrivileges(
            authorization, "/bin/launchctl", kAuthorizationFlagDefaults, argument_pointers.data(), &pipe);
#pragma clang diagnostic pop

        if (status != errAuthorizationSuccess)
        {
            if (status != errAuthorizationCanceled)
                LOG(ERROR) << "AuthorizationExecuteWithPrivileges failed:" << status;
            AuthorizationFree(authorization, kAuthorizationFlagDefaults);
            return false;
        }

        // The privileged tool's stdout is connected to |pipe|; it and the dialog that inherits the
        // descriptor keep it open until the dialog closes. Watch for EOF to learn when it finished,
        // and keep what was written, because the exit code is in there.
        QSocketNotifier* notifier = new QSocketNotifier(fileno(pipe), QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this,
                [notifier, pipe, authorization, on_finished, output = QByteArray()]() mutable
        {
            char buffer[256];
            ssize_t read_size = ::read(fileno(pipe), buffer, sizeof(buffer));
            if (read_size > 0)
            {
                output.append(buffer, read_size);
                return;
            }

            notifier->setEnabled(false);
            notifier->deleteLater();
            fclose(pipe);
            AuthorizationFree(authorization, kAuthorizationFlagDefaults);

            on_finished(exitCodeFromOutput(output));
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
ScopedQPointer<ElevateUtil> ElevateUtil::create(QObject* parent)
{
#if defined(Q_OS_WINDOWS)
    return ScopedQPointer<ElevateUtil>(new WinElevateUtil(parent));
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    return ScopedQPointer<ElevateUtil>(new LinuxElevateUtil(parent));
#elif defined(Q_OS_MACOS)
    return ScopedQPointer<ElevateUtil>(new MacElevateUtil(parent));
#else
    Q_UNUSED(parent);
    return ScopedQPointer<ElevateUtil>();
#endif // defined(Q_OS_*)
}

//--------------------------------------------------------------------------------------------------
// static
void ElevateUtil::reportExitCode(int exit_code)
{
#if defined(Q_OS_MACOS)
    // The mechanism of macOS ends with the pipe of this process and carries nothing else, so the
    // code goes into that pipe.
    std::printf("%s%d\n", kExitCodeMark, exit_code);
    std::fflush(stdout);
#else
    Q_UNUSED(exit_code);
#endif // defined(Q_OS_MACOS)
}
