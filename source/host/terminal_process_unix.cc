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

#include <cstdlib>
#include <memory>

#include <csignal>
#if defined(Q_OS_MACOS)
#include <util.h>
#else
#include <pty.h>
#endif
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
        // Child process: launch the user's shell.
        const char* shell = getenv("SHELL");
        if (!shell || !shell[0])
            shell = "/bin/bash";

        setenv("TERM", "xterm-256color", 1);

        execl(shell, shell, static_cast<char*>(nullptr));
        execl("/bin/sh", "/bin/sh", static_cast<char*>(nullptr));
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
        // Closing the master descriptor sends SIGHUP to the shell.
        ::kill(child_pid_, SIGHUP);
        ::waitpid(child_pid_, nullptr, 0);
        child_pid_ = -1;
    }
}
