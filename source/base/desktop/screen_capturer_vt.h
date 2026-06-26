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

#include <QByteArray>
#include <QRect>

#include <memory>

#include "base/desktop/screen_capturer.h"

class Frame;
class VtSession;

// Renders a Linux virtual terminal as a video frame. The kernel keeps the VT's screen content (characters
// and VGA attributes) in /dev/vcsa<N>; this reads it and rasterizes the cell grid with a built-in VGA
// font. It captures the VT owned by a VtSession (a dedicated background login terminal), so it needs no
// compositor, GPU or portal - the last fallback when no graphical capturer works. The agent runs as root.
class ScreenCapturerVt final : public ScreenCapturer
{
public:
    explicit ScreenCapturerVt(VtSession* session, QObject* parent = nullptr);
    ~ScreenCapturerVt() final;

    // Returns nullptr if |session| has no allocated VT.
    static ScreenCapturerVt* create(VtSession* session, QObject* parent = nullptr);

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
    // Renders the cell grid into |frame_|: |codepoints| (Unicode from /dev/vcsu) drive the glyphs and
    // |attributes| (from /dev/vcsa) drive the colors. Also draws the text cursor.
    void renderConsole(const quint32* codepoints, const quint8* attributes, int rows, int cols,
                       int cursor_x, int cursor_y);

    VtSession* session_;

    // FreeType-backed monospace font (with a per-code-point glyph cache) and its cell metrics.
    struct FontData;
    std::unique_ptr<FontData> font_;
    int cell_width_ = 0;
    int cell_height_ = 0;
    int cell_ascent_ = 0;

    std::unique_ptr<Frame> frame_;
    // Previous /dev/vcsa snapshot; an unchanged read yields an empty updated region so idle consoles are
    // not re-encoded every poll.
    QByteArray last_console_;

    QRect screen_rect_;
    QPoint cursor_position_;

    Q_DISABLE_COPY_MOVE(ScreenCapturerVt)
};

#endif // BASE_DESKTOP_SCREEN_CAPTURER_VT_H
