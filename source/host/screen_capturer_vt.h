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

#ifndef HOST_SCREEN_CAPTURER_VT_H
#define HOST_SCREEN_CAPTURER_VT_H

#include <QRect>

#include <memory>

#include "base/shared_pointer.h"
#include "base/desktop/vt_monitors.h"
#include "host/screen_capturer.h"

class Frame;

// Renders a set of Linux text terminals as a video frame, exposing them as switchable monitors. Each
// VtSession runs a login shell on a pseudo-terminal and emulates the terminal with libvterm; this reads the
// active terminal's character grid and rasterizes it with a monospace font (via FreeType). It needs no
// compositor, GPU or portal - the last fallback when no graphical capturer works. The agent runs as root.
class ScreenCapturerVt final : public ScreenCapturer
{
public:
    ~ScreenCapturerVt() final;

    // Starts the login terminals (owned by the capturer) and the renderer. Returns nullptr if no terminal
    // could be started.
    static ScreenCapturerVt* create(QObject* parent = nullptr);

    // The terminals exposed as switchable monitors; input and resize components are built on top of them.
    SharedPointer<VtMonitors> monitors() const { return monitors_; }

    // Pixel size of one character cell; used to map a target resolution to a terminal grid.
    QSize cellSize() const { return QSize(cell_width_, cell_height_); }

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    ScreenId currentScreen() const final;
    const Frame* captureFrame(Error* error) final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;
    const QRect& desktopRect() const final;
    const QRect& currentScreenRect() const final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

private:
    ScreenCapturerVt(SharedPointer<VtMonitors> monitors, QObject* parent);

    bool init();
    // Returns the live rendered pixel size (grid times cell). Lets screenList report the current resolution
    // directly (like desktop capturers query the OS), not a cached frame.
    QSize currentResolution() const;
    // Rasterizes |screen| (cells, colors and cursor from libvterm) into |frame_|.
    void renderConsole(const VtScreen& screen);

    SharedPointer<VtMonitors> monitors_;

    // FreeType-backed monospace font (with a per-code-point glyph cache) and its cell metrics.
    struct FontData;
    std::unique_ptr<FontData> font_;
    int cell_width_ = 0;
    int cell_height_ = 0;
    int cell_ascent_ = 0;

    std::unique_ptr<Frame> frame_;
    // Working screen snapshot and the last rendered one; an unchanged generation skips the frame entirely,
    // and a changed one is diffed cell by cell to build the updated region.
    VtScreen screen_;
    VtScreen last_screen_;
    quint64 last_generation_ = 0;

    QRect screen_rect_;
    QPoint cursor_position_;

    Q_DISABLE_COPY_MOVE(ScreenCapturerVt)
};

#endif // HOST_SCREEN_CAPTURER_VT_H
