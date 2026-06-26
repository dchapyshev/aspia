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

#include <ft2build.h>
#include FT_FREETYPE_H

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/vt_session.h"

namespace {

const size_t kFrameAlignment = 32;
const int kFontPixelSize = 16;

// Standard VGA text palette, indexed by the 4-bit color value, stored as R, G, B.
const quint8 kVgaPalette[16][3] =
{
    {   0,   0,   0 }, {   0,   0, 170 }, {   0, 170,   0 }, {   0, 170, 170 },
    { 170,   0,   0 }, { 170,   0, 170 }, { 170,  85,   0 }, { 170, 170, 170 },
    {  85,  85,  85 }, {  85,  85, 255 }, {  85, 255,  85 }, {  85, 255, 255 },
    { 255,  85,  85 }, { 255,  85, 255 }, { 255, 255,  85 }, { 255, 255, 255 }
};

// Candidate monospace fonts, tried in order; the first that loads is used.
const char* const kFontCandidates[] =
{
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    "/usr/share/fonts/liberation/LiberationMono-Regular.ttf"
};

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

    font_ = std::make_unique<FontData>();
    if (FT_Init_FreeType(&font_->library) != 0)
    {
        LOG(ERROR) << "FT_Init_FreeType failed";
        return false;
    }

    for (const char* path : kFontCandidates)
    {
        if (FT_New_Face(font_->library, path, 0, &font_->face) == 0)
        {
            LOG(INFO) << "VT font:" << path;
            break;
        }
    }

    if (!font_->face)
    {
        LOG(ERROR) << "No monospace font found";
        return false;
    }

    FT_Set_Pixel_Sizes(font_->face, 0, kFontPixelSize);

    // Cell metrics from the scaled face: monospace advance, line height and baseline (26.6 fixed-point).
    FT_Load_Char(font_->face, 'M', FT_LOAD_DEFAULT);
    cell_width_ = static_cast<int>(font_->face->glyph->advance.x >> 6);
    cell_height_ = static_cast<int>(font_->face->size->metrics.height >> 6);
    cell_ascent_ = static_cast<int>(font_->face->size->metrics.ascender >> 6);

    if (cell_width_ <= 0 || cell_height_ <= 0)
    {
        LOG(ERROR) << "Invalid font metrics";
        return false;
    }

    LOG(INFO) << "VT cell:" << cell_width_ << "x" << cell_height_;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerVt::renderConsole(const quint32* codepoints, const quint8* attributes, int rows,
                                     int cols, int cursor_x, int cursor_y)
{
    quint8* data = frame_->frameData();
    const int stride = frame_->stride();
    const int width = cols * cell_width_;
    const int height = rows * cell_height_;

    // Pass 1: fill cell backgrounds.
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const quint8 attr = attributes[(row * cols + col) * 2 + 1];
            const quint8* bg = kVgaPalette[(attr >> 4) & 0x07];

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
            const int index = row * cols + col;
            const quint32 codepoint = codepoints[index];
            if (codepoint == 0 || codepoint == ' ')
                continue;

            const FontData::Glyph* glyph = font_->glyph(codepoint);
            if (!glyph || glyph->coverage.empty())
                continue;

            const quint8* fg = kVgaPalette[attributes[index * 2 + 1] & 0x0f];
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
    if (cursor_x >= 0 && cursor_x < cols && cursor_y >= 0 && cursor_y < rows)
    {
        const quint8* fg = kVgaPalette[attributes[(cursor_y * cols + cursor_x) * 2 + 1] & 0x0f];

        for (int y = cell_height_ - 2; y < cell_height_; ++y)
        {
            quint8* line = data + (cursor_y * cell_height_ + y) * stride + cursor_x * cell_width_ * 4;
            for (int x = 0; x < cell_width_; ++x)
            {
                line[x * 4 + 0] = fg[2];
                line[x * 4 + 1] = fg[1];
                line[x * 4 + 2] = fg[0];
                line[x * 4 + 3] = 255;
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
    screen.resolution = currentResolution();
    screen.dpi = QPoint(96, 96);
    screen.is_primary = true;
    screens->screens.append(screen);
    return true;
}

//--------------------------------------------------------------------------------------------------
QSize ScreenCapturerVt::currentResolution() const
{
    const int vt = session_ ? session_->vtNumber() : -1;
    if (vt >= 0)
    {
        const QByteArray path = QByteArray("/dev/vcsa") + QByteArray::number(vt);
        const int fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
        if (fd >= 0)
        {
            // Header: lines, columns, cursor column, cursor row.
            quint8 header[4] = {};
            const ssize_t count = ::read(fd, header, sizeof(header));
            ::close(fd);

            if (count == static_cast<ssize_t>(sizeof(header)) && header[0] > 0 && header[1] > 0)
                return QSize(header[1] * cell_width_, header[0] * cell_height_);
        }
    }

    return screen_rect_.size();
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

    const QSize size(cols * cell_width_, rows * cell_height_);

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
    cursor_position_ = QPoint(cursor_x * cell_width_, cursor_y * cell_height_);
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
