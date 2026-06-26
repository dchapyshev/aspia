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

#ifndef BASE_DESKTOP_VT_SESSION_H
#define BASE_DESKTOP_VT_SESSION_H

#include <sys/types.h>

// Owns a login session on a dedicated Linux virtual terminal. It allocates a free VT, starts a getty
// (login prompt) on it, and injects keystrokes into it. The VT lives in the background - the foreground
// console is untouched - while the kernel renders its text into /dev/vcsa<N> for ScreenCapturerVt to read.
// This is the last-resort "give me a terminal" path used when no graphical capture works. Needs root.
class VtSession
{
public:
    VtSession();
    ~VtSession();

    // Allocates a free VT and starts a getty (login prompt) on it. Returns false on failure.
    bool start();

    // The allocated VT number, or -1 if not started.
    int vtNumber() const { return vt_num_; }

    // Resizes the virtual terminal grid to |rows| x |cols| (VT_RESIZE) and notifies the running program
    // (TIOCSWINSZ -> SIGWINCH). Note: VT_RESIZE is kernel-global - it changes the grid of all VTs.
    bool resize(int rows, int cols);

    // Injects |length| bytes into the VT's input queue as if typed (TIOCSTI). No-op until started.
    void sendInput(const char* data, int length);

private:
    int console_fd_ = -1;
    int tty_fd_ = -1;
    int vt_num_ = -1;
    pid_t child_pid_ = -1;

    VtSession(const VtSession&) = delete;
    VtSession& operator=(const VtSession&) = delete;
};

#endif // BASE_DESKTOP_VT_SESSION_H
