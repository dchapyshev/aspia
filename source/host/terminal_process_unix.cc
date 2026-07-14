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

#include "host/terminal_process_unix.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#include <csignal>
#if defined(Q_OS_MACOS)
#include <util.h>
#else
#include <pty.h>
#endif
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/write.hpp>

#include "base/logging.h"
#include "base/threading/asio_event_dispatcher.h"

//--------------------------------------------------------------------------------------------------
TerminalProcessUnix::TerminalProcessUnix(QObject* parent)
    : TerminalProcess(parent),
      pty_(AsioEventDispatcher::ioContext())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TerminalProcessUnix::~TerminalProcessUnix()
{
    LOG(INFO) << "Dtor";
    stop();
}

//--------------------------------------------------------------------------------------------------
bool TerminalProcessUnix::start(int columns, int rows)
{
    struct winsize window_size;
    memset(&window_size, 0, sizeof(window_size));
    window_size.ws_col = static_cast<unsigned short>(columns);
    window_size.ws_row = static_cast<unsigned short>(rows);

    int master_fd = -1;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, &window_size);
    if (pid < 0)
    {
        PLOG(ERROR) << "forkpty failed";
        return false;
    }

    if (pid == 0)
    {
        // Child process: launch the user's login shell. The agent already dropped privileges to the
        // target user, so take the shell from that account's passwd entry (getenv("SHELL") is not set
        // for a service-launched process). Start it as a login shell (argv[0] prefixed with '-') so it
        // sources the login profile - PATH additions such as Homebrew live in ~/.zprofile /
        // ~/.bash_profile and are invisible to a non-login shell.
        struct passwd* pw = getpwuid(getuid());

        const char* shell = (pw && pw->pw_shell && pw->pw_shell[0]) ? pw->pw_shell : getenv("SHELL");
        if (!shell || !shell[0])
        {
#if defined(Q_OS_MACOS)
            shell = "/bin/zsh";
#else
            shell = "/bin/bash";
#endif
        }

        setenv("SHELL", shell, 1);
        setenv("TERM", "xterm-256color", 1);

        const char* shell_base = strrchr(shell, '/');
        shell_base = shell_base ? shell_base + 1 : shell;
        const std::string login_argv0 = std::string("-") + shell_base;

        execl(shell, login_argv0.c_str(), static_cast<char*>(nullptr));
        execl("/bin/sh", "-sh", static_cast<char*>(nullptr));
        _exit(127);
    }

    child_pid_ = pid;

    std::error_code error_code;
    pty_.assign(master_fd, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Unable to assign master descriptor:" << error_code;
        ::close(master_fd);
        return false;
    }

    doRead();
    return true;
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessUnix::writeInput(const QByteArray& data)
{
    if (!pty_.is_open())
        return;

    auto buffer = std::make_shared<QByteArray>(data);
    asio::async_write(pty_, asio::buffer(buffer->constData(), buffer->size()),
                      [buffer](const std::error_code&, size_t) {});
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessUnix::resize(int columns, int rows)
{
    if (!pty_.is_open())
        return;

    struct winsize window_size;
    memset(&window_size, 0, sizeof(window_size));
    window_size.ws_col = static_cast<unsigned short>(columns);
    window_size.ws_row = static_cast<unsigned short>(rows);

    ioctl(pty_.native_handle(), TIOCSWINSZ, &window_size);
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessUnix::doRead()
{
    auto guard = alive_guard_;
    pty_.async_read_some(asio::buffer(read_buffer_),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            emit sig_finished();
            return;
        }

        emit sig_output(QByteArray(read_buffer_.data(), static_cast<int>(bytes_transferred)));
        doRead();
    });
}

//--------------------------------------------------------------------------------------------------
void TerminalProcessUnix::stop()
{
    // Prevent any already-queued read handler from touching this object.
    *alive_guard_ = false;

    std::error_code error_code;
    pty_.close(error_code);

    if (child_pid_ > 0)
    {
        // Closing the master descriptor above already sent SIGHUP to the foreground process group;
        // send it explicitly too in case the shell is not the group leader.
        ::kill(child_pid_, SIGHUP);

        // Reap the shell, but do not block teardown indefinitely: a shell that ignores SIGHUP would
        // otherwise hang here forever (this runs from the destructor). Poll for a bounded period, then
        // escalate to SIGKILL, which cannot be caught or ignored.
        constexpr int kGracefulWaitMs = 2000;
        constexpr int kStepMs = 10;

        bool reaped = false;
        for (int elapsed_ms = 0; elapsed_ms < kGracefulWaitMs; elapsed_ms += kStepMs)
        {
            const pid_t result = ::waitpid(child_pid_, nullptr, WNOHANG);
            if (result == child_pid_ || (result == -1 && errno != EINTR))
            {
                // Child reaped, or already gone (ECHILD). |result| == 0 means still running.
                reaped = true;
                break;
            }

            const struct timespec delay = { 0, kStepMs * 1000 * 1000 };
            ::nanosleep(&delay, nullptr);
        }

        if (!reaped)
        {
            ::kill(child_pid_, SIGKILL);
            ::waitpid(child_pid_, nullptr, 0);
        }

        child_pid_ = -1;
    }
}
