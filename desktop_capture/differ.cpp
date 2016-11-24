/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/differ.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/differ.h"

#include "desktop_capture/diff_block_sse2.h"
#include "base/logging.h"
#include "libyuv/cpu_id.h"

// Размер блока
static const int kBlockSize = 16;

//
// Based on WebRTC souce code
// Original files:
//  webrtc\modules\desktop_capture\differ_block.cc
//  webrtc\modules\desktop_capture\differ_block_sse2.cc
//  webrtc\modules\desktop_capture\differ.cc
//

template <const int block_size, const int bits_per_pixel>
static uint8_t
DiffFullBlock_C(const uint8_t *image1, const uint8_t *image2, int bytes_per_row)
{
    const int bytes_per_block = block_size * (bits_per_pixel / 8);

    for (int y = 0; y < block_size; ++y)
    {
        // Если строка имеет отличия
        if (memcmp(image1, image2, bytes_per_block) != 0)
        {
            return 1;
        }

        // Переходим к следующей строке изображениях
        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0;
}

//
// Check for diffs in upper-left portion of the block. The size of the portion
// to check is specified by the |width| and |height| values.
// Note that if we force the capturer to always return images whose width and
// height are multiples of kBlockSize, then this will never be called.
//
static uint8_t
DiffPartialBlock(const uint8_t *prev_image,
                 const uint8_t *curr_image,
                 int bytes_per_row,
                 int bytes_per_block,
                 int height)
{
    for (int y = 0; y < height; ++y)
    {
        if (memcmp(prev_image, curr_image, bytes_per_block) != 0)
        {
            return 1;
        }

        prev_image += bytes_per_row;
        curr_image += bytes_per_row;
    }

    return 0;
}

Differ::Differ(const DesktopSize &size, int bytes_per_pixel) :
    size_(size),
    diff_full_block_func_(nullptr)
{
    static_assert(kBlockSize == 16 || kBlockSize == 32, "Unsupported block size.");

    bytes_per_pixel_ = bytes_per_pixel;
    bytes_per_block_ = bytes_per_pixel_ * kBlockSize;
    bytes_per_row_ = bytes_per_pixel_ * size.width();

    diff_width_ = ((size.width() + kBlockSize - 1) / kBlockSize) + 1;
    diff_height_ = ((size.height() + kBlockSize - 1) / kBlockSize) + 1;
    int diff_info_size = diff_width_ * diff_height_;

    diff_info_.reset(new uint8_t[diff_info_size]);
    memset(diff_info_.get(), 0, diff_info_size);

    // Calc number of full blocks.
    full_blocks_x_ = size.width() / kBlockSize;
    full_blocks_y_ = size.height() / kBlockSize;

    // Calc size of partial blocks which may be present on right and bottom edge.
    partial_column_width_ = size.width() - (full_blocks_x_ * kBlockSize);
    partial_row_height_ = size.height() - (full_blocks_y_ * kBlockSize);

    // Offset from the start of one block-row to the next.
    block_stride_y_ = bytes_per_row_ * kBlockSize;

    // Проверяем поддерживается ли SSE2 процессором
    if (libyuv::TestCpuFlag(libyuv::kCpuHasSSE2))
    {
        // SSE2 поддерживается, используем оптимизированные функции
        DLOG(INFO) << "SSE2 supported";

        if (kBlockSize == 16)
        {
            switch (bytes_per_pixel)
            {
                case 4: diff_full_block_func_ = DiffFullBlock_16x16_32bpp_SSE2; break;
                case 2: diff_full_block_func_ = DiffFullBlock_16x16_16bpp_SSE2; break;
            }
        }
        else if (kBlockSize == 32)
        {
            switch (bytes_per_pixel)
            {
                case 4: diff_full_block_func_ = DiffFullBlock_32x32_32bpp_SSE2; break;
                case 2: diff_full_block_func_ = DiffFullBlock_32x32_16bpp_SSE2; break;
            }
        }
    }
    else
    {
        // SSE2 не поддерживается, используем обычные функции
        DLOG(INFO) << "SSE2 not supported";

        switch (bytes_per_pixel)
        {
            case 4: diff_full_block_func_ = DiffFullBlock_C<kBlockSize, 32>; break;
            case 2: diff_full_block_func_ = DiffFullBlock_C<kBlockSize, 16>; break;
        }
    }

    CHECK(diff_full_block_func_);
}

Differ::~Differ()
{
    // Nothing
}

//
// Identify all of the blocks that contain changed pixels.
//
void Differ::MarkChangedBlocks(const uint8_t *prev_image, const uint8_t *curr_image)
{
    const uint8_t *prev_block_row_start = prev_image;
    const uint8_t *curr_block_row_start = curr_image;

    // Offset from the start of one diff_info row to the next.
    int diff_stride = diff_width_;

    uint8_t *is_diff_row_start = diff_info_.get();

    for (int y = 0; y < full_blocks_y_; ++y)
    {
        const uint8_t *prev_block = prev_block_row_start;
        const uint8_t *curr_block = curr_block_row_start;

        uint8_t *is_different = is_diff_row_start;

        for (int x = 0; x < full_blocks_x_; ++x)
        {
            // Mark this block as being modified so that it gets incorporated into a dirty rect.
            *is_different = diff_full_block_func_(prev_block, curr_block, bytes_per_row_);

            prev_block += bytes_per_block_;
            curr_block += bytes_per_block_;

            ++is_different;
        }

        //
        // If there is a partial column at the end, handle it.
        // This condition should rarely, if ever, occur.
        //
        if (partial_column_width_ != 0)
        {
            *is_different = DiffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             bytes_per_block_,
                                             kBlockSize);

            ++is_different;
        }

        // Update pointers for next row.
        prev_block_row_start += block_stride_y_;
        curr_block_row_start += block_stride_y_;

        is_diff_row_start += diff_stride;
    }

    //
    // If the screen height is not a multiple of the block size, then this
    // handles the last partial row. This situation is far more common than the
    // 'partial column' case.
    //
    if (partial_row_height_ != 0)
    {
        const uint8_t *prev_block = prev_block_row_start;
        const uint8_t *curr_block = curr_block_row_start;

        uint8_t *is_different = is_diff_row_start;

        for (int x = 0; x < full_blocks_x_; ++x)
        {
            *is_different = DiffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             bytes_per_block_,
                                             partial_row_height_);

            prev_block += bytes_per_block_;
            curr_block += bytes_per_block_;
            ++is_different;
        }

        if (partial_column_width_ != 0)
        {
            *is_different = DiffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             partial_column_width_ * bytes_per_pixel_,
                                             partial_row_height_);
        }
    }
}

//
// After the dirty blocks have been identified, this routine merges adjacent
// blocks into a region.
// The goal is to minimize the region that covers the dirty blocks.
//
void Differ::MergeChangedBlocks(DesktopRegion &changed_region)
{
    changed_region.Clear();

    uint8_t *is_diff_row_start = diff_info_.get();
    int diff_stride = diff_width_;

    for (int y = 0; y < diff_height_; ++y)
    {
        uint8_t *is_different = is_diff_row_start;

        for (int x = 0; x < diff_width_; ++x)
        {
            //
            // We've found a modified block. Look at blocks to the right and below
            // to group this block with as many others as we can.
            //
            if (*is_different != 0)
            {
                int width  = 1; // Ширина прямоугольника в блоках
                int height = 1; // Высота прямоугольника в блоках

                *is_different = 0;

                //
                // Group with blocks to the right.
                // We can keep looking until we find an unchanged block because we
                // have a boundary block which is never marked as having diffs.
                //
                uint8_t *right = is_different + 1;

                while (*right != 0)
                {
                    *right++ = 0;
                    ++width;
                }

                //
                // Group with blocks below.
                // The entire width of blocks that we matched above much match for
                // each row that we add.
                //
                uint8_t *bottom = is_different;
                bool found_new_row;

                do
                {
                    found_new_row = true;
                    bottom += diff_stride;
                    right = bottom;

                    for (int x2 = 0; x2 < width; ++x2)
                    {
                        if (*right++ == 0)
                            found_new_row = false;
                    }

                    if (found_new_row)
                    {
                        ++height;

                        //
                        // We need to go back and erase the diff markers so that we don't
                        // try to add these blocks a second time.
                        //
                        right = bottom;

                        for (int x2 = 0; x2 < width; ++x2)
                        {
                            *right++ = 0;
                        }
                    }
                } while (found_new_row);

                int32_t l = x * kBlockSize;
                int32_t t = y * kBlockSize;
                int32_t r = l + (width * kBlockSize);
                int32_t b = t + (height * kBlockSize);

                if (r > size_.width()) r = size_.width();
                if (b > size_.height()) b = size_.height();

                // Add rect to region.
                changed_region.AddRect(DesktopRect::MakeLTRB(l, t, r, b));
            }

            // Increment to next block in this row.
            ++is_different;
        }

        // Go to start of next row.
        is_diff_row_start += diff_stride;
    }
}

void Differ::CalcChangedRegion(const uint8_t *prev_image,
                               const uint8_t *curr_image,
                               DesktopRegion &changed_region)
{
    // Identify all the blocks that contain changed pixels.
    MarkChangedBlocks(prev_image, curr_image);

    //
    // Now that we've identified the blocks that have changed, merge adjacent
    // blocks to minimize the number of rects that we return.
    //
    MergeChangedBlocks(changed_region);
}
