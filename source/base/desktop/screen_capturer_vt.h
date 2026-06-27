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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_VT_H
#define BASE_DESKTOP_SCREEN_CAPTURER_VT_H

#include <QRect>

#include <memory>

#include "base/desktop/screen_capturer.h"
#include "base/desktop/vt_monitors.h"

class Frame;

// Renders a set of Linux text terminals as a video frame, exposing them as switchable monitors. Each
// VtSession runs a login shell on a pseudo-terminal and emulates the terminal with libvterm; this reads the
// active terminal's character grid and rasterizes it with a monospace font (via FreeType). It needs no
// compositor, GPU or portal - the last fallback when no graphical capturer works. The agent runs as root.
class ScreenCapturerVt final : public ScreenCapturer
{
public:
    explicit ScreenCapturerVt(VtMonitors* monitors, QObject* parent = nullptr);
    ~ScreenCapturerVt() final;

    // Returns nullptr if no terminal is available.
    static ScreenCapturerVt* create(VtMonitors* monitors, QObject* parent = nullptr);

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
    bool init();
    // Returns the live rendered pixel size (grid times cell). Lets screenList report the current resolution
    // directly (like desktop capturers query the OS), not a cached frame.
    QSize currentResolution() const;
    // Rasterizes |screen| (cells, colors and cursor from libvterm) into |frame_|.
    void renderConsole(const VtScreen& screen);

    VtMonitors* monitors_;

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

#endif // BASE_DESKTOP_SCREEN_CAPTURER_VT_H
