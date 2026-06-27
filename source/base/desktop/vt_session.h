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

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QSocketNotifier>

#include <string>
#include <vector>

#include "base/scoped_qpointer.h"

struct VTerm;
struct VTermScreen;
struct VTermState;

// One rendered character cell: a code point and its resolved RGB colors (reverse video already folded in).
struct VtCell
{
    char32_t ch = 0; // 0 = blank
    quint8 fg[3] = { 0, 0, 0 };
    quint8 bg[3] = { 0, 0, 0 };

    bool operator==(const VtCell&) const = default;
};

// A snapshot of the terminal screen for the renderer.
struct VtScreen
{
    int cols = 0;
    int rows = 0;
    int cursor_col = 0;
    int cursor_row = 0;
    quint64 generation = 0; // bumped on every libvterm change; lets the renderer skip idle frames

    // Highlighted text selection (cell coordinates, reading order), drawn as reverse video.
    bool has_selection = false;
    int sel_start_col = 0;
    int sel_start_row = 0;
    int sel_end_col = 0;
    int sel_end_row = 0;

    std::vector<VtCell> cells; // rows * cols, row-major
};

// Non-printable keys the injector can send (mapped to the terminal's escape sequences by libvterm).
enum class VtKey
{
    ENTER, TAB, BACKSPACE, ESCAPE, UP, DOWN, LEFT, RIGHT, INSERT, DELETE, HOME, END, PAGE_UP, PAGE_DOWN
};

// Owns a login session on a pseudo-terminal and emulates a terminal over it with libvterm. A login prompt
// runs on the PTY slave (a fresh session asking for login and password); the PTY output is read on the
// event loop (a socket notifier) and fed into libvterm, which maintains the screen the renderer reads.
// Keyboard and mouse input go through libvterm too, so it produces the correct escape sequences for the
// running application. Everything runs on one thread, so libvterm needs no locking. This is the last-resort
// "give me a terminal" path used when no graphical capture works. Needs root (to spawn login).
class VtSession final : public QObject
{
    Q_OBJECT

public:
    explicit VtSession(QObject* parent = nullptr);
    ~VtSession() final;

    // Opens a PTY and starts a login prompt on it, plus the emulator. Returns false on failure.
    bool start();

    // True once a terminal is running.
    bool isRunning() const { return master_fd_ >= 0; }

    // Current terminal geometry in character cells.
    bool consoleSize(int* cols, int* rows) const;

    // Resizes the terminal grid and the PTY window (SIGWINCH to the running program).
    bool resize(int rows, int cols);

    // Copies the current screen into |out|. Returns false if no terminal is running.
    bool captureScreen(VtScreen* out);

    // Extracts the text covered by a selection (cell coordinates, inclusive) as UTF-8, with trailing spaces
    // trimmed per line and rows joined by newlines.
    std::string selectionText(int start_col, int start_row, int end_col, int end_row);

    // Sets / clears the highlighted text selection (cell coordinates). The capturer reads it for rendering.
    void setSelection(int start_col, int start_row, int end_col, int end_row);
    void clearSelection();

    // Input. Mouse coordinates are in character cells (0-based; libvterm adds the protocol offset).
    // Everything funnels through libvterm, which emits nothing when the application has not enabled the
    // relevant mode.
    void inputUnichar(char32_t ch, bool ctrl, bool alt);
    void inputKey(VtKey key, bool shift, bool ctrl, bool alt);
    void inputMouseMove(int col, int row);
    void inputMouseButton(int button, bool pressed);

    // Writes |text| (UTF-8) to the terminal as if pasted.
    void paste(const std::string& text);

    // True while the running application has mouse reporting enabled.
    bool mouseActive() const { return mouse_active_; }
    // Internal: updated from the libvterm settermprop callback.
    void updateMouseActive(bool active) { mouse_active_ = active; }
    // Internal: bumped from the libvterm damage / cursor callbacks on every visible change.
    void notifyChanged() { ++generation_; }

private slots:
    void onReadable();
    void onWritable();

private:
    // Opens a fresh PTY, forks a login prompt on it and arms the notifiers. Used by start() and on respawn.
    bool spawnLogin();
    // Tears down the dead PTY, clears the screen and spawns a new login prompt.
    void restartLogin();
    // Schedules restartLogin(), throttled so a failing login cannot busy-loop.
    void scheduleRestart();
    void writeMaster(const char* data, int length);
    // Drains libvterm's pending output (query replies, key and mouse reports) to the PTY.
    void flushOutput();

    int master_fd_ = -1; // PTY master
    pid_t child_pid_ = -1;

    VTerm* vt_ = nullptr;
    VTermScreen* screen_ = nullptr;
    VTermState* state_ = nullptr;

    ScopedQPointer<QSocketNotifier> read_notifier_;
    ScopedQPointer<QSocketNotifier> write_notifier_;
    QByteArray pending_write_; // output that did not fit the PTY buffer, flushed on the write notifier

    bool mouse_active_ = false;
    quint64 generation_ = 0;
    QElapsedTimer spawn_timer_; // measures time since the last login spawn, for throttling respawns

    // Highlighted text selection in cell coordinates (ordered in reading order).
    bool has_selection_ = false;
    int sel_start_col_ = 0;
    int sel_start_row_ = 0;
    int sel_end_col_ = 0;
    int sel_end_row_ = 0;

    Q_DISABLE_COPY_MOVE(VtSession)
};

#endif // BASE_DESKTOP_VT_SESSION_H
