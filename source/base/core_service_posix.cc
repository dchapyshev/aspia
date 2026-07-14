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

#include "base/core_service.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/logging.h"

namespace {

volatile sig_atomic_t g_signal_write_fd = -1;

//--------------------------------------------------------------------------------------------------
QString sigToString(int sig)
{
    switch (sig)
    {
        case SIGKILL:
            return "SIGKILL";
        case SIGTERM:
            return "SIGTERM";
        case SIGHUP:
            return "SIGHUP";
        case SIGQUIT:
            return "SIGQUIT";
        case SIGINT:
            return "SIGINT";
        case SIGSTOP:
            return "SIGSTOP";
        case SIGABRT:
            return "SIGABRT";
        default:
            return "unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
CoreService::CoreService(const QString& name, QObject* parent)
    : name_(name)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
CoreService::~CoreService()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
int CoreService::exec(CoreApplication& application)
{
    LOG(INFO) << "Begin";

    std::unique_ptr<QSocketNotifier> signal_notifier;

    // A signal handler may run at any moment, so it must be async-signal-safe. It only writes the
    // signal number to this self-pipe; the actual handling happens in onSignalActivated() on the
    // event loop thread.
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signal_fd_) != 0)
    {
        PLOG(ERROR) << "socketpair failed";
    }
    else
    {
        // Non-blocking so the handler never blocks; close-on-exec so the descriptors do not leak
        // into child processes.
        for (int fd : signal_fd_)
        {
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
        }

        g_signal_write_fd = signal_fd_[0];

        signal_notifier = std::make_unique<QSocketNotifier>(signal_fd_[1], QSocketNotifier::Read);
        connect(signal_notifier.get(), &QSocketNotifier::activated, this, &CoreService::onSignalActivated);

        // Install the handlers only when the self-pipe exists. Otherwise the default disposition is
        // kept, so the process can still be terminated by SIGTERM/SIGINT/SIGQUIT instead of trapping
        // them in a handler that can no longer do anything.
        if (signal(SIGKILL, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGKILL";

        if (signal(SIGTERM, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGTERM";

        if (signal(SIGHUP, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGHUP";

        if (signal(SIGQUIT, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGQUIT";

        if (signal(SIGINT, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGINT";

        if (signal(SIGSTOP, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGSTOP";

        if (signal(SIGABRT, signalHandler) == SIG_ERR)
            LOG(ERROR) << "Unable to install signal handler for SIGABRT";
    }

    QTimer::singleShot(0, this, &CoreService::onStart);

    LOG(INFO) << "Run message loop";
    int ret = application.exec();

    // Stop routing signals through the pipe and release the descriptors.
    g_signal_write_fd = -1;

    // Destroy the notifier before closing the descriptor it watches.
    signal_notifier.reset();

    if (signal_fd_[0] != -1)
    {
        ::close(signal_fd_[0]);
        signal_fd_[0] = -1;
    }

    if (signal_fd_[1] != -1)
    {
        ::close(signal_fd_[1]);
        signal_fd_[1] = -1;
    }

    LOG(INFO) << "End";
    return ret;
}

//--------------------------------------------------------------------------------------------------
void CoreService::onSignalActivated()
{
    bool stop_requested = false;
    unsigned char buffer[32];
    ssize_t count;

    // Drain the pipe: several signals may have been coalesced into it.
    while ((count = ::read(signal_fd_[1], buffer, sizeof(buffer))) > 0)
    {
        for (ssize_t i = 0; i < count; ++i)
        {
            int sig = static_cast<int>(buffer[i]);
            LOG(INFO) << "Signal received: " << sigToString(sig) << " (" << sig << ")";

            if (sig == SIGTERM || sig == SIGINT || sig == SIGQUIT)
                stop_requested = true;
        }
    }

    if (stop_requested)
    {
        // A message loop termination command was received.
        onStop();
        QCoreApplication::quit();
    }
}

//--------------------------------------------------------------------------------------------------
// static
void CoreService::signalHandler(int sig)
{
    // Async-signal-safe: forward the signal number through the self-pipe and return. Anything heavier
    // (logging, memory allocation, Qt calls) would be unsafe in this context.
    if (g_signal_write_fd == -1)
        return;

    const int saved_errno = errno;
    const unsigned char byte = static_cast<unsigned char>(sig);

    ssize_t rv;
    do
    {
        rv = ::write(g_signal_write_fd, &byte, 1);
    }
    while (rv == -1 && errno == EINTR);

    errno = saved_errno;
}
