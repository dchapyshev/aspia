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

#include <QSocketNotifier>
#include <QTimer>

#include <vterm.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
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

//--------------------------------------------------------------------------------------------------
int onSetTermProp(VTermProp prop, VTermValue* value, void* user)
{
    if (prop == VTERM_PROP_MOUSE)
        static_cast<VtSession*>(user)->updateMouseActive(value->number != VTERM_PROP_MOUSE_NONE);
    return 1;
}

//--------------------------------------------------------------------------------------------------
int onDamage(VTermRect /* rect */, void* user)
{
    static_cast<VtSession*>(user)->notifyChanged();
    return 1;
}

//--------------------------------------------------------------------------------------------------
int onMoveCursor(VTermPos /* pos */, VTermPos /* oldpos */, int /* visible */, void* user)
{
    static_cast<VtSession*>(user)->notifyChanged();
    return 1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
VtSession::VtSession(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
VtSession::~VtSession()
{
    // Stop reacting to the PTY (notifiers must not watch a closed fd) before tearing down the emulator.
    if (read_notifier_)
        read_notifier_->setEnabled(false);
    if (write_notifier_)
        write_notifier_->setEnabled(false);
    read_notifier_.reset();
    write_notifier_.reset();

    if (child_pid_ > 0)
    {
        // Kill the login/shell process group.
        ::kill(-child_pid_, SIGKILL);
        ::kill(child_pid_, SIGKILL);

        int status = 0;
        ::waitpid(child_pid_, &status, 0);
    }

    if (vt_)
        vterm_free(vt_);

    if (master_fd_ >= 0)
        ::close(master_fd_);
}

//--------------------------------------------------------------------------------------------------
bool VtSession::start()
{
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

    // damage / movecursor bump a generation counter so the renderer re-renders exactly when libvterm
    // changes. settermprop tracks the mouse-reporting mode. libvterm keeps a pointer to the callbacks, so
    // the table must outlive the terminal.
    static VTermScreenCallbacks screen_callbacks = {};
    screen_callbacks.damage = onDamage;
    screen_callbacks.movecursor = onMoveCursor;
    screen_callbacks.settermprop = onSetTermProp;
    vterm_screen_set_callbacks(screen_, &screen_callbacks, this);

    VTermColor fg;
    VTermColor bg;
    vterm_color_rgb(&fg, 0xcc, 0xcc, 0xcc);
    vterm_color_rgb(&bg, 0x00, 0x00, 0x00);
    vterm_state_set_default_colors(state_, &fg, &bg);

    return spawnLogin();
}

//--------------------------------------------------------------------------------------------------
bool VtSession::spawnLogin()
{
    // Open a PTY: the login prompt runs on the slave, we drive the master. Non-blocking so the read/write
    // notifiers never stall the event loop.
    master_fd_ = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd_ < 0 || ::grantpt(master_fd_) != 0 || ::unlockpt(master_fd_) != 0)
    {
        PLOG(ERROR) << "Unable to open pty";
        return false;
    }

    const int flags = ::fcntl(master_fd_, F_GETFL, 0);
    ::fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);

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

    // Match the PTY window size to the emulator grid (preserved across respawns).
    int rows = kDefaultRows;
    int cols = kDefaultCols;
    if (vt_)
        vterm_get_size(vt_, &rows, &cols);

    struct winsize ws = {};
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);
    ::ioctl(master_fd_, TIOCSWINSZ, &ws);

    read_notifier_ = new QSocketNotifier(master_fd_, QSocketNotifier::Read);
    connect(read_notifier_.get(), &QSocketNotifier::activated, this, &VtSession::onReadable);

    write_notifier_ = new QSocketNotifier(master_fd_, QSocketNotifier::Write);
    write_notifier_->setEnabled(false);
    connect(write_notifier_.get(), &QSocketNotifier::activated, this, &VtSession::onWritable);

    spawn_timer_.start();
    LOG(INFO) << "VT login prompt started (pid" << child_pid_ << ")";
    return true;
}

//--------------------------------------------------------------------------------------------------
void VtSession::restartLogin()
{
    if (read_notifier_)
        read_notifier_->setEnabled(false);
    if (write_notifier_)
        write_notifier_->setEnabled(false);
    read_notifier_.reset();
    write_notifier_.reset();
    pending_write_.clear();

    if (child_pid_ > 0)
    {
        ::kill(-child_pid_, SIGKILL);
        ::kill(child_pid_, SIGKILL);

        int status = 0;
        ::waitpid(child_pid_, &status, 0);
        child_pid_ = -1;
    }

    if (master_fd_ >= 0)
    {
        ::close(master_fd_);
        master_fd_ = -1;
    }

    // Clear the leftover screen (e.g. the "login timed out" message) so the fresh prompt starts clean.
    if (vt_)
    {
        const char clear[] = "\x1b[H\x1b[2J";
        vterm_input_write(vt_, clear, sizeof(clear) - 1);
    }

    spawnLogin();
}

//--------------------------------------------------------------------------------------------------
void VtSession::scheduleRestart()
{
    const qint64 elapsed = spawn_timer_.elapsed();
    const int delay = (elapsed >= 1000) ? 0 : static_cast<int>(1000 - elapsed);

    QTimer::singleShot(delay, this, [this]() { restartLogin(); });
}

//--------------------------------------------------------------------------------------------------
bool VtSession::consoleSize(int* cols, int* rows) const
{
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
    if (!vt_)
        return false;

    vterm_set_size(vt_, rows, cols);
    flushOutput();

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
    if (!vt_)
        return false;

    int rows = 0;
    int cols = 0;
    vterm_get_size(vt_, &rows, &cols);

    out->rows = rows;
    out->cols = cols;
    out->generation = generation_;
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

    out->has_selection = has_selection_;
    out->sel_start_col = sel_start_col_;
    out->sel_start_row = sel_start_row_;
    out->sel_end_col = sel_end_col_;
    out->sel_end_row = sel_end_row_;
    return true;
}

//--------------------------------------------------------------------------------------------------
std::string VtSession::selectionText(int start_col, int start_row, int end_col, int end_row)
{
    if (!vt_)
        return {};

    int rows = 0;
    int cols = 0;
    vterm_get_size(vt_, &rows, &cols);

    // Order the endpoints top-to-bottom (and left-to-right within a single row).
    if (start_row > end_row || (start_row == end_row && start_col > end_col))
    {
        std::swap(start_row, end_row);
        std::swap(start_col, end_col);
    }

    start_row = std::clamp(start_row, 0, rows - 1);
    end_row = std::clamp(end_row, 0, rows - 1);
    start_col = std::clamp(start_col, 0, cols - 1);
    end_col = std::clamp(end_col, 0, cols - 1);

    std::string result;
    std::vector<char> buffer(static_cast<size_t>(cols) * 4 + 1);

    for (int row = start_row; row <= end_row; ++row)
    {
        const int from = (row == start_row) ? start_col : 0;
        const int to = (row == end_row) ? end_col : cols - 1;

        VTermRect rect;
        rect.start_row = row;
        rect.end_row = row + 1;
        rect.start_col = from;
        rect.end_col = to + 1;

        const size_t count = vterm_screen_get_text(screen_, buffer.data(), buffer.size() - 1, rect);

        std::string line(buffer.data(), count);
        const size_t last = line.find_last_not_of(' ');
        line = (last == std::string::npos) ? std::string() : line.substr(0, last + 1);

        if (row != start_row)
            result += '\n';
        result += line;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void VtSession::setSelection(int start_col, int start_row, int end_col, int end_row)
{
    // Order the endpoints in reading order (top-to-bottom, left-to-right).
    if (start_row > end_row || (start_row == end_row && start_col > end_col))
    {
        std::swap(start_row, end_row);
        std::swap(start_col, end_col);
    }

    sel_start_col_ = start_col;
    sel_start_row_ = start_row;
    sel_end_col_ = end_col;
    sel_end_row_ = end_row;
    has_selection_ = true;
    notifyChanged();
}

//--------------------------------------------------------------------------------------------------
void VtSession::clearSelection()
{
    if (!has_selection_)
        return;

    has_selection_ = false;
    notifyChanged();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputUnichar(char32_t ch, bool ctrl, bool alt)
{
    if (!vt_)
        return;
    vterm_keyboard_unichar(vt_, ch, modifiers(false, ctrl, alt));
    flushOutput();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputKey(VtKey key, bool shift, bool ctrl, bool alt)
{
    if (!vt_)
        return;
    vterm_keyboard_key(vt_, vtermKey(key), modifiers(shift, ctrl, alt));
    flushOutput();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputFunctionKey(int number, bool shift, bool ctrl, bool alt)
{
    if (!vt_)
        return;
    vterm_keyboard_key(vt_, static_cast<VTermKey>(VTERM_KEY_FUNCTION(number)), modifiers(shift, ctrl, alt));
    flushOutput();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputMouseMove(int col, int row)
{
    if (!vt_)
        return;
    vterm_mouse_move(vt_, row, col, VTERM_MOD_NONE);
    flushOutput();
}

//--------------------------------------------------------------------------------------------------
void VtSession::inputMouseButton(int button, bool pressed)
{
    if (!vt_)
        return;
    vterm_mouse_button(vt_, button, pressed, VTERM_MOD_NONE);
    flushOutput();
}

//--------------------------------------------------------------------------------------------------
void VtSession::paste(const std::string& text)
{
    writeMaster(text.data(), static_cast<int>(text.size()));
}

//--------------------------------------------------------------------------------------------------
void VtSession::onReadable()
{
    char buf[4096];

    for (;;)
    {
        const ssize_t count = ::read(master_fd_, buf, sizeof(buf));
        if (count > 0)
        {
            vterm_input_write(vt_, buf, static_cast<size_t>(count));
            flushOutput();
        }
        else if (count < 0 && errno == EINTR)
        {
            continue;
        }
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break; // No more data right now; wait for the next readable notification.
        }
        else
        {
            // EOF (count == 0), or on Linux EIO once the slave side closes: the login/shell exited
            // (e.g. login timed out). Respawn a fresh login prompt.
            read_notifier_->setEnabled(false);
            scheduleRestart();
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void VtSession::onWritable()
{
    while (!pending_write_.isEmpty())
    {
        const ssize_t count = ::write(master_fd_, pending_write_.constData(), pending_write_.size());
        if (count > 0)
        {
            pending_write_.remove(0, static_cast<int>(count));
        }
        else if (count < 0 && errno == EINTR)
        {
            continue;
        }
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return; // Still full; wait for the next writable notification.
        }
        else
        {
            pending_write_.clear();
            break;
        }
    }

    write_notifier_->setEnabled(false);
}

//--------------------------------------------------------------------------------------------------
void VtSession::writeMaster(const char* data, int length)
{
    if (master_fd_ < 0)
        return;

    // Preserve order: if output is already queued, append to it instead of writing directly.
    if (!pending_write_.isEmpty())
    {
        pending_write_.append(data, length);
        return;
    }

    int offset = 0;
    while (offset < length)
    {
        const ssize_t count = ::write(master_fd_, data + offset, length - offset);
        if (count > 0)
        {
            offset += static_cast<int>(count);
        }
        else if (count < 0 && errno == EINTR)
        {
            continue;
        }
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            pending_write_.append(data + offset, length - offset);
            write_notifier_->setEnabled(true);
            return;
        }
        else
        {
            PLOG(ERROR) << "Unable to write to pty";
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void VtSession::flushOutput()
{
    char out[1024];
    size_t count;
    while ((count = vterm_output_read(vt_, out, sizeof(out))) > 0)
        writeMaster(out, static_cast<int>(count));
}
