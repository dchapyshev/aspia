//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/differ.h"

#include "base/logging.h"
#include "base/desktop/diff_block_32bpp_sse2.h"
#include "base/desktop/diff_block_32bpp_c.h"

#include <cstring>
#include <libyuv/cpu_id.h>

namespace base {

namespace {

const int kBlockSize = 16;
const int kBytesPerPixel = 4;
const int kBytesPerBlock = kBlockSize * kBytesPerPixel;

//--------------------------------------------------------------------------------------------------
// Check for diffs in upper-left portion of the block. The size of the portion to check is
// specified by the |width| and |height| values.
// Note that if we force the capturer to always return images whose width and height are multiples
// of kBlockSize, then this will never be called.
quint8 diffPartialBlock(const quint8* prev_image,
                         const quint8* curr_image,
                         int bytes_per_row,
                         int bytes_per_block,
                         int height)
{
    for (int y = 0; y < height; ++y)
    {
        if (memcmp(prev_image, curr_image, static_cast<size_t>(bytes_per_block)) != 0)
            return 1U;

        prev_image += bytes_per_row;
        curr_image += bytes_per_row;
    }

    return 0U;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Differ::Differ(const Size& size)
    : screen_rect_(Rect::makeSize(size)),
      bytes_per_row_(size.width() * kBytesPerPixel),
      diff_width_(((size.width() + kBlockSize - 1) / kBlockSize) + 1),
      diff_height_(((size.height() + kBlockSize - 1) / kBlockSize) + 1),
      full_blocks_x_(size.width() / kBlockSize),
      full_blocks_y_(size.height() / kBlockSize)
{
    LOG(LS_INFO) << "Screen size: " << size;
    LOG(LS_INFO) << "Bytes per row: " << bytes_per_row_;
    LOG(LS_INFO) << "Diff size: " << diff_width_ << "x" << diff_height_;
    LOG(LS_INFO) << "Full blocks: " << full_blocks_x_ << "x" << full_blocks_y_;

    const size_t diff_info_size = static_cast<size_t>(diff_width_ * diff_height_);

    diff_info_ = std::make_unique<quint8[]>(diff_info_size);
    memset(diff_info_.get(), 0, diff_info_size);

    // Calc size of partial blocks which may be present on right and bottom edge.
    partial_column_width_ = size.width() - (full_blocks_x_ * kBlockSize);
    partial_row_height_ = size.height() - (full_blocks_y_ * kBlockSize);

    LOG(LS_INFO) << "Partial column: " << partial_column_width_;
    LOG(LS_INFO) << "Partial row: " << partial_row_height_;

    // Offset from the start of one block-row to the next.
    block_stride_y_ = bytes_per_row_ * kBlockSize;

    LOG(LS_INFO) << "Block stride: " << block_stride_y_;

    diff_full_block_func_ = diffFunction();
    CHECK(diff_full_block_func_);
}

//--------------------------------------------------------------------------------------------------
// static
Differ::DiffFullBlockFunc Differ::diffFunction()
{
    if (libyuv::TestCpuFlag(libyuv::kCpuHasSSE2))
    {
#if defined(Q_PROCESSOR_X86)
        LOG(LS_INFO) << "SSE2 differ loaded";

        if constexpr (kBlockSize == 16)
            return diffFullBlock_32bpp_16x16_SSE2;
        else if constexpr (kBlockSize == 32)
            return diffFullBlock_32bpp_32x32_SSE2;
#endif // defined(Q_PROCESSOR_X86)
    }
    else
    {
        LOG(LS_INFO) << "C differ loaded";

        if constexpr (kBlockSize == 16)
            return diffFullBlock_32bpp_16x16_C;
        else if constexpr (kBlockSize == 32)
            return diffFullBlock_32bpp_32x32_C;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
// Identify all of the blocks that contain changed pixels.
void Differ::markDirtyBlocks(const quint8* prev_image, const quint8* curr_image)
{
    const quint8* prev_block_row_start = prev_image;
    const quint8* curr_block_row_start = curr_image;

    // Offset from the start of one diff_info row to the next.
    const int diff_stride = diff_width_;

    quint8* is_diff_row_start = diff_info_.get();

    for (int y = 0; y < full_blocks_y_; ++y)
    {
        const quint8* prev_block = prev_block_row_start;
        const quint8* curr_block = curr_block_row_start;

        quint8* is_different = is_diff_row_start;

        for (int x = 0; x < full_blocks_x_; ++x)
        {
            // Mark this block as being modified so that it gets incorporated into a dirty rect.
            *is_different = diff_full_block_func_(prev_block, curr_block, bytes_per_row_);

            prev_block += kBytesPerBlock;
            curr_block += kBytesPerBlock;

            ++is_different;
        }

        // If there is a partial column at the end, handle it. This condition should rarely, if
        // ever, occur.
        if (partial_column_width_ != 0)
        {
            *is_different = diffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             partial_column_width_ * kBytesPerPixel,
                                             kBlockSize);
        }

        // Update pointers for next row.
        prev_block_row_start += block_stride_y_;
        curr_block_row_start += block_stride_y_;

        is_diff_row_start += diff_stride;
    }

    // If the screen height is not a multiple of the block size, then this handles the last partial
    // row. This situation is far more common than the 'partial column' case.
    if (partial_row_height_ != 0)
    {
        const quint8* prev_block = prev_block_row_start;
        const quint8* curr_block = curr_block_row_start;

        quint8* is_different = is_diff_row_start;

        for (int x = 0; x < full_blocks_x_; ++x)
        {
            *is_different = diffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             kBytesPerBlock,
                                             partial_row_height_);

            prev_block += kBytesPerBlock;
            curr_block += kBytesPerBlock;
            ++is_different;
        }

        if (partial_column_width_ != 0)
        {
            *is_different =
                diffPartialBlock(prev_block,
                                 curr_block,
                                 bytes_per_row_,
                                 partial_column_width_ * kBytesPerPixel,
                                 partial_row_height_);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// After the dirty blocks have been identified, this routine merges adjacent blocks into a region.
// The goal is to minimize the region that covers the dirty blocks.
void Differ::mergeBlocks(Region* dirty_region)
{
    quint8* is_diff_row_start = diff_info_.get();
    const int diff_stride = diff_width_;

    for (int y = 0; y < diff_height_; ++y)
    {
        quint8* is_different = is_diff_row_start;

        for (int x = 0; x < diff_width_; ++x)
        {
            // We've found a modified block. Look at blocks to the right and below to group this
            // block with as many others as we can.
            if (*is_different != 0)
            {
                // Width and height of the rectangle in blocks.
                int width  = 1;
                int height = 1;

                *is_different = 0;

                // Group with blocks to the right. We can keep looking until we find an unchanged
                // block because we have a boundary block which is never marked as having diffs.
                quint8* right = is_different + 1;

                while (*right != 0)
                {
                    *right++ = 0;
                    ++width;
                }

                // Group with blocks below. The entire width of blocks that we matched above much
                // match for each row that we add.
                quint8* bottom = is_different;
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

                        // We need to go back and erase the diff markers so that we don't try to
                        // add these blocks a second time.
                        right = bottom;

                        for (int x2 = 0; x2 < width; ++x2)
                        {
                            *right++ = 0;
                        }
                    }
                } while (found_new_row);

                Rect dirty_rect = Rect::makeXYWH(x * kBlockSize, y * kBlockSize,
                                                 width * kBlockSize, height * kBlockSize);

                dirty_rect.intersectWith(screen_rect_);

                // Add rect to region.
                dirty_region->addRect(dirty_rect);
            }

            // Increment to next block in this row.
            ++is_different;
        }

        // Go to start of next row.
        is_diff_row_start += diff_stride;
    }
}

//--------------------------------------------------------------------------------------------------
void Differ::calcDirtyRegion(const quint8* prev_image,
                             const quint8* curr_image,
                             Region* dirty_region)
{
    dirty_region->clear();

    // Identify all the blocks that contain changed pixels.
    markDirtyBlocks(prev_image, curr_image);

    // Now that we've identified the blocks that have changed, merge adjacent blocks to minimize
    // the number of rects that we return.
    mergeBlocks(dirty_region);
}

} // namespace base
