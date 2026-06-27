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

#include <QFile>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/region.h"

namespace {

const size_t kFrameAlignment = 32;
const int kFontPixelSize = 16;

// Fixed character cell; the (smaller) glyph is rendered inside it. Round cell dimensions keep the rendered
// size a clean multiple, so the selectable resolutions land on standard values.
const int kCellWidth = 10;
const int kCellHeight = 20;

//--------------------------------------------------------------------------------------------------
// True if cell (|row|, |col|) is inside |screen|'s text selection (reading-order range).
bool cellSelected(const VtScreen& screen, int row, int col)
{
    if (!screen.has_selection)
        return false;
    const bool after_start = (row > screen.sel_start_row) ||
                             (row == screen.sel_start_row && col >= screen.sel_start_col);
    const bool before_end = (row < screen.sel_end_row) ||
                            (row == screen.sel_end_row && col <= screen.sel_end_col);
    return after_start && before_end;
}

} // namespace

//--------------------------------------------------------------------------------------------------
struct ScreenCapturerVt::FontData
{
    struct Glyph
    {
        int width = 0;
        int height = 0;
        int left = 0;
        int top = 0;
        std::vector<quint8> coverage; // 8-bit alpha, width * height
    };

    FT_Library library = nullptr;
    FT_Face face = nullptr;
    QByteArray font_data; // backs FT_New_Memory_Face; must outlive the face
    std::unordered_map<char32_t, Glyph> cache;

    ~FontData()
    {
        if (face)
            FT_Done_Face(face);
        if (library)
            FT_Done_FreeType(library);
    }

    // Rasterizes |codepoint| (cached). The returned glyph may be empty if the font has no such glyph.
    const Glyph* glyph(char32_t codepoint)
    {
        const auto it = cache.find(codepoint);
        if (it != cache.end())
            return &it->second;

        Glyph glyph;
        if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER) == 0)
        {
            const FT_GlyphSlot slot = face->glyph;
            const FT_Bitmap& bitmap = slot->bitmap;

            glyph.width = static_cast<int>(bitmap.width);
            glyph.height = static_cast<int>(bitmap.rows);
            glyph.left = slot->bitmap_left;
            glyph.top = slot->bitmap_top;
            glyph.coverage.resize(static_cast<size_t>(glyph.width) * glyph.height);

            for (int y = 0; y < glyph.height; ++y)
            {
                memcpy(glyph.coverage.data() + static_cast<size_t>(y) * glyph.width,
                       bitmap.buffer + static_cast<size_t>(y) * bitmap.pitch,
                       static_cast<size_t>(glyph.width));
            }
        }

        return &cache.emplace(codepoint, std::move(glyph)).first->second;
    }
};

//--------------------------------------------------------------------------------------------------
ScreenCapturerVt::ScreenCapturerVt(SharedPointer<VtMonitors> monitors, QObject* parent)
    : ScreenCapturer(Type::LINUX_VT, parent),
      monitors_(std::move(monitors))
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
ScreenCapturerVt* ScreenCapturerVt::create(QObject* parent)
{
    // Two login terminals, exposed to the client as switchable monitors.
    std::vector<ScopedQPointer<VtSession>> sessions;
    for (int i = 0; i < 2; ++i)
    {
        ScopedQPointer<VtSession> session(new VtSession());
        if (session->start())
            sessions.push_back(std::move(session));
    }

    if (sessions.empty())
    {
        LOG(ERROR) << "No VT session could be started";
        return nullptr;
    }

    SharedPointer<VtMonitors> monitors(new VtMonitors(std::move(sessions)));

    std::unique_ptr<ScreenCapturerVt> self(new ScreenCapturerVt(std::move(monitors), parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::init()
{
    if (!monitors_ || monitors_->count() == 0)
    {
        LOG(ERROR) << "No VT session to capture";
        return false;
    }

    font_ = std::make_unique<FontData>();
    if (FT_Init_FreeType(&font_->library) != 0)
    {
        LOG(ERROR) << "FT_Init_FreeType failed";
        return false;
    }

    // The font is embedded in the application resources, so it works on systems without installed fonts.
    QFile font_file(":/fonts/JetBrainsMono-Regular.ttf");
    if (!font_file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open the embedded font";
        return false;
    }
    font_->font_data = font_file.readAll();

    if (FT_New_Memory_Face(font_->library,
                           reinterpret_cast<const FT_Byte*>(font_->font_data.constData()),
                           font_->font_data.size(), 0, &font_->face) != 0)
    {
        LOG(ERROR) << "FT_New_Memory_Face failed";
        return false;
    }

    FT_Set_Pixel_Sizes(font_->face, 0, kFontPixelSize);

    // Use a fixed cell; place the glyph on the baseline, vertically centered in the cell (26.6 fixed-point).
    const int font_height = static_cast<int>(font_->face->size->metrics.height >> 6);
    const int ascender = static_cast<int>(font_->face->size->metrics.ascender >> 6);
    cell_width_ = kCellWidth;
    cell_height_ = kCellHeight;
    cell_ascent_ = (cell_height_ - font_height) / 2 + ascender;

    LOG(INFO) << "VT cell:" << cell_width_ << "x" << cell_height_;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerVt::renderConsole(const VtScreen& screen)
{
    quint8* data = frame_->frameData();
    const int stride = frame_->stride();
    const int rows = screen.rows;
    const int cols = screen.cols;
    const int width = cols * cell_width_;
    const int height = rows * cell_height_;

    // Pass 1: fill cell backgrounds (frame is BGRA, cell colors are RGB). Selected cells are drawn in
    // reverse video (foreground and background swapped).
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const VtCell& cell = screen.cells[static_cast<size_t>(row) * cols + col];
            const quint8* bg = cellSelected(screen, row, col) ? cell.fg : cell.bg;

            for (int y = 0; y < cell_height_; ++y)
            {
                quint8* line = data + (row * cell_height_ + y) * stride + col * cell_width_ * 4;
                for (int x = 0; x < cell_width_; ++x)
                {
                    line[x * 4 + 0] = bg[2];
                    line[x * 4 + 1] = bg[1];
                    line[x * 4 + 2] = bg[0];
                    line[x * 4 + 3] = 255;
                }
            }
        }
    }

    // Pass 2: rasterize and alpha-blend glyphs over the backgrounds.
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const VtCell& cell = screen.cells[static_cast<size_t>(row) * cols + col];
            if (cell.ch == 0 || cell.ch == ' ')
                continue;

            const FontData::Glyph* glyph = font_->glyph(cell.ch);
            if (!glyph || glyph->coverage.empty())
                continue;

            const quint8* fg = cellSelected(screen, row, col) ? cell.bg : cell.fg;
            const int origin_x = col * cell_width_ + glyph->left;
            const int origin_y = row * cell_height_ + cell_ascent_ - glyph->top;

            for (int gy = 0; gy < glyph->height; ++gy)
            {
                const int py = origin_y + gy;
                if (py < 0 || py >= height)
                    continue;

                quint8* line = data + py * stride;
                const quint8* coverage = glyph->coverage.data() + static_cast<size_t>(gy) * glyph->width;

                for (int gx = 0; gx < glyph->width; ++gx)
                {
                    const int px = origin_x + gx;
                    if (px < 0 || px >= width)
                        continue;

                    const quint8 alpha = coverage[gx];
                    if (!alpha)
                        continue;

                    quint8* pixel = line + px * 4;
                    pixel[0] = static_cast<quint8>((fg[2] * alpha + pixel[0] * (255 - alpha)) / 255);
                    pixel[1] = static_cast<quint8>((fg[1] * alpha + pixel[1] * (255 - alpha)) / 255);
                    pixel[2] = static_cast<quint8>((fg[0] * alpha + pixel[2] * (255 - alpha)) / 255);
                }
            }
        }
    }

    // Text cursor: a solid underline over the bottom two rows of its cell.
    const int cursor_x = screen.cursor_col;
    const int cursor_y = screen.cursor_row;
    if (cursor_x >= 0 && cursor_x < cols && cursor_y >= 0 && cursor_y < rows)
    {
        const VtCell& cell = screen.cells[static_cast<size_t>(cursor_y) * cols + cursor_x];

        for (int y = cell_height_ - 2; y < cell_height_; ++y)
        {
            quint8* line = data + (cursor_y * cell_height_ + y) * stride + cursor_x * cell_width_ * 4;
            for (int x = 0; x < cell_width_; ++x)
            {
                line[x * 4 + 0] = cell.fg[2];
                line[x * 4 + 1] = cell.fg[1];
                line[x * 4 + 2] = cell.fg[0];
                line[x * 4 + 3] = 255;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerVt::screenCount()
{
    return monitors_ ? monitors_->count() : 0;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::screenList(ScreenList* screens)
{
    screens->screens.clear();
    screens->resolutions.clear();

    // One monitor per terminal, laid out left to right with each one's own resolution.
    int offset_x = 0;
    for (int i = 0; i < monitors_->count(); ++i)
    {
        VtSession* session = monitors_->session(i);

        int cols = 0;
        int rows = 0;
        const QSize resolution = (session && session->consoleSize(&cols, &rows))
            ? QSize(cols * cell_width_, rows * cell_height_) : QSize();

        Screen screen;
        screen.id = i;
        screen.position = QPoint(offset_x, 0);
        screen.resolution = resolution;
        screen.dpi = QPoint(96, 96);
        screen.is_primary = (i == 0);
        screens->screens.append(screen);

        offset_x += resolution.width();
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
QSize ScreenCapturerVt::currentResolution() const
{
    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;

    int cols = 0;
    int rows = 0;
    if (session && session->consoleSize(&cols, &rows))
        return QSize(cols * cell_width_, rows * cell_height_);

    return screen_rect_.size();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerVt::selectScreen(ScreenId screen_id)
{
    if (screen_id == kInvalidScreenId)
        return true;
    if (screen_id < 0 || screen_id >= monitors_->count())
        return false;

    monitors_->setActive(static_cast<int>(screen_id));
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerVt::currentScreen() const
{
    return monitors_ ? monitors_->active() : 0;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerVt::captureFrame(Error* error)
{
    DCHECK(error);
    *error = Error::TEMPORARY;

    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;
    if (!session || !session->captureScreen(&screen_))
        return nullptr;

    if (screen_.rows <= 0 || screen_.cols <= 0)
        return nullptr;

    const QSize size(screen_.cols * cell_width_, screen_.rows * cell_height_);

    // Re-render only when libvterm reported a change (generation; bumped for content, cursor and selection);
    // otherwise report an empty region so the encoder skips the frame.
    if (frame_ && frame_->size() == size && screen_.generation == last_generation_)
    {
        frame_->updatedRegion()->clear();
        *error = Error::SUCCEEDED;
        return frame_.get();
    }

    const bool size_changed = !frame_ || frame_->size() != size;
    if (size_changed)
    {
        frame_ = FrameAligned::create(size, kFrameAlignment);
        if (!frame_)
            return nullptr;
        frame_->setCapturerType(static_cast<quint32>(type()));
    }

    renderConsole(screen_);

    screen_rect_ = QRect(QPoint(0, 0), size);
    cursor_position_ = QPoint(screen_.cursor_col * cell_width_, screen_.cursor_row * cell_height_);

    Region* region = frame_->updatedRegion();
    region->clear();

    if (size_changed || last_screen_.cols != screen_.cols || last_screen_.rows != screen_.rows)
    {
        // First frame or a geometry change: everything is dirty.
        *region += screen_rect_;
    }
    else
    {
        // A cell's pixels depend on its content, its selection state and whether the cursor sits on it;
        // mark per-row spans where any of those differs from the previously rendered screen.
        for (int row = 0; row < screen_.rows; ++row)
        {
            int first = -1;
            int last = -1;

            for (int col = 0; col < screen_.cols; ++col)
            {
                const size_t index = static_cast<size_t>(row) * screen_.cols + col;
                const bool was_cursor =
                    (last_screen_.cursor_row == row && last_screen_.cursor_col == col);
                const bool is_cursor = (screen_.cursor_row == row && screen_.cursor_col == col);

                const bool dirty = !(screen_.cells[index] == last_screen_.cells[index]) ||
                                   cellSelected(screen_, row, col) != cellSelected(last_screen_, row, col) ||
                                   is_cursor != was_cursor;
                if (dirty)
                {
                    if (first < 0)
                        first = col;
                    last = col;
                }
            }

            if (first >= 0)
            {
                *region += QRect(first * cell_width_, row * cell_height_,
                                 (last - first + 1) * cell_width_, cell_height_);
            }
        }
    }

    last_generation_ = screen_.generation;
    last_screen_ = screen_;

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
    last_generation_ = 0;
    last_screen_ = VtScreen();
}
