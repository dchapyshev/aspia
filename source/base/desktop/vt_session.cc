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

#include <QByteArray>

#include <vterm.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <utility>

#include "base/logging.h"

namespace {

const char kLoginPath[] = "/bin/login";

// Initial terminal size: a mid-range standard resolution (1280 x 720 with the renderer's 10x20 cell).
const int kDefaultCols = 128;
const int kDefaultRows = 36;

//--------------------------------------------------------------------------------------------------
VTermModifier modifiers(bool shift, bool ctrl, bool alt)
{
    int mod = VTERM_MOD_NONE;
    if (shift) mod |= VTERM_MOD_SHIFT;
    if (ctrl)  mod |= VTERM_MOD_CTRL;
    if (alt)   mod |= VTERM_MOD_ALT;
    return static_cast<VTermModifier>(mod);
}

//--------------------------------------------------------------------------------------------------
VTermKey vtermKey(VtKey key)
{
    switch (key)
    {
        case VtKey::ENTER:     return VTERM_KEY_ENTER;
        case VtKey::TAB:       return VTERM_KEY_TAB;
        case VtKey::BACKSPACE: return VTERM_KEY_BACKSPACE;
        case VtKey::ESCAPE:    return VTERM_KEY_ESCAPE;
        case VtKey::UP:        return VTERM_KEY_UP;
        case VtKey::DOWN:      return VTERM_KEY_DOWN;
        case VtKey::LEFT:      return VTERM_KEY_LEFT;
        case VtKey::RIGHT:     return VTERM_KEY_RIGHT;
        case VtKey::INSERT:    return VTERM_KEY_INS;
        case VtKey::DELETE:    return VTERM_KEY_DEL;
        case VtKey::HOME:      return VTERM_KEY_HOME;
        case VtKey::END:       return VTERM_KEY_END;
        case VtKey::PAGE_UP:   return VTERM_KEY_PAGEUP;
        case VtKey::PAGE_DOWN: return VTERM_KEY_PAGEDOWN;
    }
    return VTERM_KEY_NONE;
}

} // namespace

//--------------------------------------------------------------------------------------------------
VtSession::VtSession() = default;

//--------------------------------------------------------------------------------------------------
VtSession::~VtSession()
{
    stop_.store(true);

    if (child_pid_ > 0)
    {
        // Kill the login/shell process group; closing the slave makes the pump's master read return EOF.
        ::kill(-child_pid_, SIGKILL);
        ::kill(child_pid_, SIGKILL);

        int status = 0;
        ::waitpid(child_pid_, &status, 0);
    }

    if (pump_thread_.joinable())
        pump_thread_.join();

    if (vt_)
        vterm_free(vt_);

    if (master_fd_ >= 0)
        ::close(master_fd_);
}

//--------------------------------------------------------------------------------------------------
bool VtSession::start()
{
    // Open a PTY: the login session runs on the slave, we drive the master.
    master_fd_ = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd_ < 0 || ::grantpt(master_fd_) != 0 || ::unlockpt(master_fd_) != 0)
    {
        PLOG(ERROR) << "Unable to open pty";
        return false;
    }

    const char* slave_name = ::ptsname(master_fd_);
    if (!slave_name)
    {
        PLOG(ERROR) << "ptsname failed";
        return false;
    }

    const QByteArray slave_path = slave_name;

    const pid_t pid = ::fork();
    if (pid < 0)
    {
        PLOG(ERROR) << "fork failed";
        return false;
    }

    if (pid == 0)
    {
        // Child: own a new session on the PTY slave as the controlling terminal, then run the login prompt.
        ::setsid();

        const int slave = ::open(slave_path.constData(), O_RDWR);
        if (slave < 0)
            _exit(127);

        ::ioctl(slave, TIOCSCTTY, 0);
        ::dup2(slave, STDIN_FILENO);
        ::dup2(slave, STDOUT_FILENO);
        ::dup2(slave, STDERR_FILENO);
        if (slave > STDERR_FILENO)
            ::close(slave);
        ::close(master_fd_);

        ::setenv("TERM", "xterm", 1);
        ::execl(kLoginPath, "login", static_cast<char*>(nullptr));
        _exit(127);
    }

    child_pid_ = pid;

    // Set up the terminal emulator.
    vt_ = vterm_new(kDefaultRows, kDefaultCols);
    if (!vt_)
    {
        LOG(ERROR) << "vterm_new failed";
        return false;
    }
    vterm_set_utf8(vt_, 1);

    screen_ = vterm_obtain_screen(vt_);
    state_ = vterm_obtain_state(vt_);

    vterm_screen_reset(screen_, 1);
    vterm_screen_enable_altscreen(screen_, 1);

    VTermColor fg;
    VTermColor bg;
    vterm_color_rgb(&fg, 0xcc, 0xcc, 0xcc);
    vterm_color_rgb(&bg, 0x00, 0x00, 0x00);
    vterm_state_set_default_colors(state_, &fg, &bg);

    // Match the PTY window size to the emulator grid.
    struct winsize ws = {};
    ws.ws_row = kDefaultRows;
    ws.ws_col = kDefaultCols;
    ::ioctl(master_fd_, TIOCSWINSZ, &ws);

    pump_thread_ = std::thread(&VtSession::pumpLoop, this);

    LOG(INFO) << "VT login session started (pid" << child_pid_ << ")";
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VtSession::consoleSize(int* cols, int* rows) const
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return false;

    int r = 0;
    int c = 0;
    vterm_get_size(vt_, &r, &c);

    if (rows)
        *rows = r;
    if (cols)
        *cols = c;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VtSession::resize(int rows, int cols)
{
    {
        std::scoped_lock lock(mutex_);
        if (!vt_)
            return false;
        vterm_set_size(vt_, rows, cols);
        flushOutputLocked();
    }

    // Resize the PTY too so the running program reflows (SIGWINCH).
    struct winsize ws = {};
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);
    if (::ioctl(master_fd_, TIOCSWINSZ, &ws) != 0)
        PLOG(ERROR) << "ioctl(TIOCSWINSZ) failed";

    LOG(INFO) << "VT resized to" << cols << "x" << rows;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VtSession::captureScreen(VtScreen* out)
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return false;

    int rows = 0;
    int cols = 0;
    vterm_get_size(vt_, &rows, &cols);

    out->rows = rows;
    out->cols = cols;
    out->cells.assign(static_cast<size_t>(rows) * cols, VtCell());

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            VTermPos pos;
            pos.row = row;
            pos.col = col;

            VTermScreenCell cell;
            if (!vterm_screen_get_cell(screen_, pos, &cell))
                continue;

            VtCell& dst = out->cells[static_cast<size_t>(row) * cols + col];
            dst.ch = cell.chars[0];

            VTermColor fg = cell.fg;
            VTermColor bg = cell.bg;
            vterm_screen_convert_color_to_rgb(screen_, &fg);
            vterm_screen_convert_color_to_rgb(screen_, &bg);

            if (cell.attrs.reverse)
                std::swap(fg, bg);

            dst.fg[0] = fg.rgb.red;
            dst.fg[1] = fg.rgb.green;
            dst.fg[2] = fg.rgb.blue;
            dst.bg[0] = bg.rgb.red;
            dst.bg[1] = bg.rgb.green;
            dst.bg[2] = bg.rgb.blue;
        }
    }

    VTermPos cursor;
    vterm_state_get_cursorpos(state_, &cursor);
    out->cursor_row = cursor.row;
    out->cursor_col = cursor.col;
    return true;
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputUnichar(char32_t ch, bool ctrl, bool alt)
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return;
    vterm_keyboard_unichar(vt_, ch, modifiers(false, ctrl, alt));
    flushOutputLocked();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputKey(VtKey key, bool shift, bool ctrl, bool alt)
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return;
    vterm_keyboard_key(vt_, vtermKey(key), modifiers(shift, ctrl, alt));
    flushOutputLocked();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputMouseMove(int col, int row)
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return;
    vterm_mouse_move(vt_, row, col, VTERM_MOD_NONE);
    flushOutputLocked();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputMouseButton(int button, bool pressed)
{
    std::scoped_lock lock(mutex_);
    if (!vt_)
        return;
    vterm_mouse_button(vt_, button, pressed, VTERM_MOD_NONE);
    flushOutputLocked();
}

//--------------------------------------------------------------------------------------------------
void VtSession::pumpLoop()
{
    char buf[4096];

    while (!stop_.load(std::memory_order_relaxed))
    {
        const ssize_t count = ::read(master_fd_, buf, sizeof(buf));
        if (count > 0)
        {
            std::scoped_lock lock(mutex_);
            vterm_input_write(vt_, buf, static_cast<size_t>(count));
            flushOutputLocked();
        }
        else if (count < 0 && errno == EINTR)
        {
            continue;
        }
        else
        {
            break; // EOF (session ended) or error.
        }
    }
}

//--------------------------------------------------------------------------------------------------
void VtSession::flushOutputLocked()
{
    char out[1024];
    size_t count;
    while ((count = vterm_output_read(vt_, out, sizeof(out))) > 0)
        writeMaster(out, static_cast<int>(count));
}

//--------------------------------------------------------------------------------------------------
void VtSession::writeMaster(const char* data, int length)
{
    if (master_fd_ < 0)
        return;

    int offset = 0;
    while (offset < length)
    {
        const ssize_t count = ::write(master_fd_, data + offset, length - offset);
        if (count > 0)
            offset += static_cast<int>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }
}
