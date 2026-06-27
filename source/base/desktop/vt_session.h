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

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct VTerm;
struct VTermScreen;
struct VTermState;

// One rendered character cell: a code point and its resolved RGB colors (reverse video already folded in).
struct VtCell
{
    char32_t ch = 0; // 0 = blank
    std::uint8_t fg[3] = { 0, 0, 0 };
    std::uint8_t bg[3] = { 0, 0, 0 };

    bool operator==(const VtCell&) const = default;
};

// A snapshot of the terminal screen for the renderer.
struct VtScreen
{
    int cols = 0;
    int rows = 0;
    int cursor_col = 0;
    int cursor_row = 0;
    std::vector<VtCell> cells; // rows * cols, row-major
};

// Non-printable keys the injector can send (mapped to the terminal's escape sequences by libvterm).
enum class VtKey
{
    ENTER, TAB, BACKSPACE, ESCAPE, UP, DOWN, LEFT, RIGHT, INSERT, DELETE, HOME, END, PAGE_UP, PAGE_DOWN
};

// Owns a login session on a pseudo-terminal and emulates a terminal over it with libvterm. A login prompt
// runs on the PTY slave (a fresh session asking for login and password); a pump thread feeds the PTY output
// into libvterm, which maintains the screen the renderer reads. Keyboard and mouse input go through libvterm
// too, so it produces the correct escape sequences (cursor-key modes, mouse reporting, ...) for the running
// application. This is the last-resort "give me a terminal" path used when no graphical capture works.
// Needs root (to spawn login).
class VtSession
{
public:
    VtSession();
    ~VtSession();

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
    bool mouseActive() const { return mouse_active_.load(std::memory_order_relaxed); }
    // Internal: updated from the libvterm settermprop callback.
    void updateMouseActive(bool active) { mouse_active_.store(active, std::memory_order_relaxed); }

private:
    void pumpLoop();
    void writeMaster(const char* data, int length);
    // Drains libvterm's pending output (query replies, key and mouse reports) to the PTY. Caller holds the
    // lock.
    void flushOutputLocked();

    int master_fd_ = -1; // PTY master
    pid_t child_pid_ = -1;

    VTerm* vt_ = nullptr;
    VTermScreen* screen_ = nullptr;
    VTermState* state_ = nullptr;

    std::thread pump_thread_;
    std::atomic_bool stop_{false};
    std::atomic_bool mouse_active_{false};

    // Guards the libvterm instance: the pump thread feeds it while input and capture touch it from the agent
    // thread.
    mutable std::mutex mutex_;

    VtSession(const VtSession&) = delete;
    VtSession& operator=(const VtSession&) = delete;
};

#endif // BASE_DESKTOP_VT_SESSION_H
