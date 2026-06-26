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

#include "base/desktop/screen_capturer_vt.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/vt_session.h"

namespace {

const size_t kFrameAlignment = 32;

// The console is rendered from /dev/vcsu (real Unicode code points) using a built-in 8x16 font and its
// unicode table, not the kernel's own font: KDFONTOP is unreliable (it fails on background / graphics-mode
// VTs), and the embedded Uni2 font covers Latin, Cyrillic and Greek.
const int kFontWidth = 8;
const int kFontHeight = 16;

#include "base/desktop/vt_console_font.inc"

// Standard VGA text palette, indexed by the 4-bit color value, stored as R, G, B.
const quint8 kVgaPalette[16][3] =
{
    {   0,   0,   0 }, {   0,   0, 170 }, {   0, 170,   0 }, {   0, 170, 170 },
    { 170,   0,   0 }, { 170,   0, 170 }, { 170,  85,   0 }, { 170, 170, 170 },
    {  85,  85,  85 }, {  85,  85, 255 }, {  85, 255,  85 }, {  85, 255, 255 },
    { 255,  85,  85 }, { 255,  85, 255 }, { 255, 255,  85 }, { 255, 255, 255 }
};

//--------------------------------------------------------------------------------------------------
// Maps a Unicode code point to a font glyph index via the embedded font's unicode table, or -1 if the
// font has no glyph for it.
int glyphForCodepoint(quint32 codepoint)
{
    int low = 0;
    int high = kConsoleUniMapCount - 1;
    while (low <= high)
    {
        const int mid = (low + high) / 2;
        const quint32 entry = kConsoleUniMap[mid].codepoint;
        if (entry == codepoint)
            return kConsoleUniMap[mid].glyph;
        if (entry < codepoint)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return -1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerVt::ScreenCapturerVt(VtSession* session, QObject* parent)
    : ScreenCapturer(Type::LINUX_VT, parent),
      session_(session)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerVt::~ScreenCapturerVt()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerVt* ScreenCapturerVt::create(VtSession* session, QObject* parent)
{
    std::unique_ptr<ScreenCapturerVt> self(new ScreenCapturerVt(session, parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::init()
{
    if (!session_ || session_->vtNumber() < 0)
    {
        LOG(ERROR) << "No VT session to capture";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerVt::renderConsole(const quint32* codepoints, const quint8* attributes, int rows,
                                     int cols, int cursor_x, int cursor_y)
{
    quint8* data = frame_->frameData();
    const int stride = frame_->stride();

    static const quint8 kBlankGlyph[16] = {};

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const int index = row * cols + col;
            const quint8 attr = attributes[index * 2 + 1];

            const quint8* fg = kVgaPalette[attr & 0x0F];
            const quint8* bg = kVgaPalette[(attr >> 4) & 0x07];

            const int glyph_index = glyphForCodepoint(codepoints[index]);
            const quint8* glyph = (glyph_index >= 0) ? kConsoleFont[glyph_index] : kBlankGlyph;

            // The text cursor is drawn as a solid underline over the bottom two scan-lines of its cell.
            const bool cursor_cell = (row == cursor_y && col == cursor_x);

            for (int sy = 0; sy < kFontHeight; ++sy)
            {
                quint8* line = data + (row * kFontHeight + sy) * stride + col * kFontWidth * 4;
                const bool underline = cursor_cell && sy >= kFontHeight - 2;
                const quint8 bits = glyph[sy];

                for (int sx = 0; sx < kFontWidth; ++sx)
                {
                    const bool on = (bits >> (7 - sx)) & 1;
                    const quint8* color = (underline || on) ? fg : bg;

                    line[sx * 4 + 0] = color[2]; // B
                    line[sx * 4 + 1] = color[1]; // G
                    line[sx * 4 + 2] = color[0]; // R
                    line[sx * 4 + 3] = 255;      // A
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerVt::screenCount()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    Screen screen;
    screen.id = 0;
    screen.position = QPoint(0, 0);
    screen.resolution = screen_rect_.size();
    screen.is_primary = true;
    screens->screens.append(screen);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::selectScreen(ScreenId screen_id)
{
    return screen_id == 0 || screen_id == kInvalidScreenId;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerVt::currentScreen() const
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerVt::captureFrame(Error* error)
{
    DCHECK(error);
    *error = Error::TEMPORARY;

    const int vt = session_ ? session_->vtNumber() : -1;
    if (vt < 0)
        return nullptr;

    const QByteArray path = QByteArray("/dev/vcsa") + QByteArray::number(vt);
    const int fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return nullptr;

    // Header: lines, columns, cursor column, cursor row. The cells follow as (character, attribute).
    quint8 header[4] = {};
    if (::read(fd, header, sizeof(header)) != static_cast<ssize_t>(sizeof(header)))
    {
        ::close(fd);
        return nullptr;
    }

    const int rows = header[0];
    const int cols = header[1];
    const int cursor_x = header[2];
    const int cursor_y = header[3];

    if (rows <= 0 || cols <= 0)
    {
        ::close(fd);
        return nullptr;
    }

    const int cells_size = rows * cols * 2;
    QByteArray cells(cells_size, 0);
    int received = 0;
    while (received < cells_size)
    {
        const ssize_t count = ::read(fd, cells.data() + received, cells_size - received);
        if (count > 0)
            received += static_cast<int>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }
    ::close(fd);

    if (received != cells_size)
        return nullptr;

    // Unicode code points (4 bytes per cell, no header) from /dev/vcsu; the geometry comes from vcsa.
    const QByteArray uni_path = QByteArray("/dev/vcsu") + QByteArray::number(vt);
    const int uni_fd = ::open(uni_path.constData(), O_RDONLY | O_CLOEXEC);
    if (uni_fd < 0)
        return nullptr;

    const int uni_size = rows * cols * 4;
    QByteArray uni(uni_size, 0);
    received = 0;
    while (received < uni_size)
    {
        const ssize_t count = ::read(uni_fd, uni.data() + received, uni_size - received);
        if (count > 0)
            received += static_cast<int>(count);
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }
    ::close(uni_fd);

    if (received != uni_size)
        return nullptr;

    // Snapshot of geometry, content and cursor. An identical poll needs no re-render and reports an
    // empty region so the encoder skips the unchanged frame.
    QByteArray snapshot;
    snapshot.reserve(static_cast<int>(sizeof(header)) + cells_size + uni_size);
    snapshot.append(reinterpret_cast<const char*>(header), sizeof(header));
    snapshot.append(cells);
    snapshot.append(uni);

    const QSize size(cols * kFontWidth, rows * kFontHeight);

    if (frame_ && frame_->size() == size && snapshot == last_console_)
    {
        frame_->updatedRegion()->clear();
        *error = Error::SUCCEEDED;
        return frame_.get();
    }

    if (!frame_ || frame_->size() != size)
    {
        frame_ = FrameAligned::create(size, kFrameAlignment);
        if (!frame_)
            return nullptr;
        frame_->setCapturerType(static_cast<quint32>(type()));
    }

    renderConsole(reinterpret_cast<const quint32*>(uni.constData()),
                  reinterpret_cast<const quint8*>(cells.constData()), rows, cols, cursor_x, cursor_y);

    screen_rect_ = QRect(QPoint(0, 0), size);
    cursor_position_ = QPoint(cursor_x * kFontWidth, cursor_y * kFontHeight);
    *frame_->updatedRegion() = screen_rect_;
    last_console_ = snapshot;

    *error = Error::SUCCEEDED;
    return frame_.get();
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerVt::captureCursor()
{
    // The text cursor is drawn directly into the frame; there is no separate hardware cursor.
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerVt::cursorPosition()
{
    return cursor_position_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerVt::desktopRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerVt::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerVt::reset()
{
    last_console_.clear();
}
