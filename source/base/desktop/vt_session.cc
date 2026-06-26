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

#include "base/desktop/vt_session.h"

#include <fcntl.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <QByteArray>

#include "base/logging.h"

namespace {

const char kGettyPath[] = "/sbin/agetty";

} // namespace

//--------------------------------------------------------------------------------------------------
VtSession::VtSession() = default;

//--------------------------------------------------------------------------------------------------
VtSession::~VtSession()
{
    if (child_pid_ > 0)
    {
        // Kill the getty/login/shell process group, then free the VT (the hang-up finishes any stragglers).
        ::kill(-child_pid_, SIGKILL);
        ::kill(child_pid_, SIGKILL);

        int status = 0;
        ::waitpid(child_pid_, &status, 0);
    }

    if (tty_fd_ >= 0)
        ::close(tty_fd_);

    if (console_fd_ >= 0)
    {
        if (vt_num_ > 0)
            ::ioctl(console_fd_, VT_DISALLOCATE, vt_num_);
        ::close(console_fd_);
    }
}

//--------------------------------------------------------------------------------------------------
bool VtSession::start()
{
    console_fd_ = ::open("/dev/tty0", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (console_fd_ < 0)
    {
        PLOG(ERROR) << "Unable to open /dev/tty0";
        return false;
    }

    if (::ioctl(console_fd_, VT_OPENQRY, &vt_num_) != 0 || vt_num_ <= 0)
    {
        PLOG(ERROR) << "ioctl(VT_OPENQRY) failed";
        return false;
    }

    const QByteArray tty_name = "tty" + QByteArray::number(vt_num_);

    const pid_t pid = ::fork();
    if (pid < 0)
    {
        PLOG(ERROR) << "fork failed";
        return false;
    }

    if (pid == 0)
    {
        // Child: become a session leader and let agetty open the VT, set it as the controlling terminal
        // and run the login prompt. --noclear keeps the existing screen content for the first capture.
        ::setsid();
        ::execl(kGettyPath, "agetty", "--noclear", tty_name.constData(), "linux",
                static_cast<char*>(nullptr));
        _exit(127);
    }

    child_pid_ = pid;

    // A separate handle to the VT, used to inject input with TIOCSTI (works on a background VT as root).
    const QByteArray tty_path = "/dev/" + tty_name;
    tty_fd_ = ::open(tty_path.constData(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (tty_fd_ < 0)
    {
        PLOG(ERROR) << "Unable to open" << tty_path.constData() << "for input";
    }
    else
    {
        // Put the VT in UTF-8 mode so /dev/vcsu reports real Unicode code points for the renderer.
        const char utf8_mode[] = "\x1b%G";
        if (::write(tty_fd_, utf8_mode, sizeof(utf8_mode) - 1) < 0)
            PLOG(ERROR) << "Unable to set VT UTF-8 mode";
    }

    LOG(INFO) << "VT login session started on" << tty_name.constData() << "(pid" << child_pid_ << ")";
    return true;
}

//--------------------------------------------------------------------------------------------------
void VtSession::sendInput(const char* data, int length)
{
    if (tty_fd_ < 0)
        return;

    for (int i = 0; i < length; ++i)
    {
        if (::ioctl(tty_fd_, TIOCSTI, &data[i]) != 0)
        {
            PLOG(ERROR) << "ioctl(TIOCSTI) failed";
            break;
        }
    }
}
