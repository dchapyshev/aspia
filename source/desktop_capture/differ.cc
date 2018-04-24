//
// PROJECT:         Aspia
// FILE:            desktop_capture/differ.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/differ.h"

#include "desktop_capture/diff_block_sse2.h"

namespace aspia {

namespace {

constexpr int kBytesPerPixel = 4;
constexpr int kBlockSize = 8;
constexpr int kBytesPerBlock = kBytesPerPixel * kBlockSize;

quint8 DiffFullBlock_C(const quint8* image1, const quint8* image2, int bytes_per_row)
{
    for (int y = 0; y < kBlockSize; ++y)
    {
        if (memcmp(image1, image2, kBytesPerBlock) != 0)
        {
            return 1U;
        }

        image1 += bytes_per_row;
        image2 += bytes_per_row;
    }

    return 0U;
}

//
// Check for diffs in upper-left portion of the block. The size of the portion
// to check is specified by the |width| and |height| values.
// Note that if we force the capturer to always return images whose width and
// height are multiples of kBlockSize, then this will never be called.
//
quint8 DiffPartialBlock(const quint8* prev_image,
                        const quint8* curr_image,
                        int bytes_per_row,
                        int bytes_per_block,
                        int height)
{
    for (int y = 0; y < height; ++y)
    {
        if (memcmp(prev_image, curr_image, bytes_per_block) != 0)
            return 1U;

        prev_image += bytes_per_row;
        curr_image += bytes_per_row;
    }

    return 0U;
}

} // namespace

Differ::Differ(const QSize& size)
    : screen_rect_(QPoint(0, 0), size),
      bytes_per_row_(size.width() * kBytesPerPixel),
      diff_width_(((size.width() + kBlockSize - 1) / kBlockSize) + 1),
      diff_height_(((size.height() + kBlockSize - 1) / kBlockSize) + 1),
      full_blocks_x_(size.width() / kBlockSize),
      full_blocks_y_(size.height() / kBlockSize)
{
    const int diff_info_size = diff_width_ * diff_height_;

    diff_info_ = std::make_unique<quint8[]>(diff_info_size);
    memset(diff_info_.get(), 0, diff_info_size);

    // Calc size of partial blocks which may be present on right and bottom edge.
    partial_column_width_ = size.width() - (full_blocks_x_ * kBlockSize);
    partial_row_height_ = size.height() - (full_blocks_y_ * kBlockSize);

    // Offset from the start of one block-row to the next.
    block_stride_y_ = bytes_per_row_ * kBlockSize;
}

//
// Identify all of the blocks that contain changed pixels.
//
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
            // For x86 and x86_64, we do not support processors that do not
            // have SSE2 instructions support.
            if constexpr (kBlockSize == 8)
            {
                // Mark this block as being modified so that it gets
                // incorporated into a dirty rect.
                *is_different = diffFullBlock_8x8_SSE2(prev_block,
                                                       curr_block,
                                                       bytes_per_row_);
            }
            else if constexpr(kBlockSize == 16)
            {
                // Mark this block as being modified so that it gets
                // incorporated into a dirty rect.
                *is_different = diffFullBlock_16x16_SSE2(prev_block,
                                                         curr_block,
                                                         bytes_per_row_);
            }
            else
            {
                static_assert(kBlockSize != 32);

                // Mark this block as being modified so that it gets
                // incorporated into a dirty rect.
                *is_different = diffFullBlock_32x32_SSE2(prev_block,
                                                         curr_block,
                                                         bytes_per_row_);
            }

            prev_block += kBytesPerBlock;
            curr_block += kBytesPerBlock;

            ++is_different;
        }

        // If there is a partial column at the end, handle it.
        // This condition should rarely, if ever, occur.
        if (partial_column_width_ != 0)
        {
            *is_different = DiffPartialBlock(prev_block,
                                             curr_block,
                                             bytes_per_row_,
                                             kBytesPerBlock,
                                             kBlockSize);
        }

        // Update pointers for next row.
        prev_block_row_start += block_stride_y_;
        curr_block_row_start += block_stride_y_;

        is_diff_row_start += diff_stride;
    }

    // If the screen height is not a multiple of the block size, then this
    // handles the last partial row. This situation is far more common than
    // the 'partial column' case.
    if (partial_row_height_ != 0)
    {
        const quint8* prev_block = prev_block_row_start;
        const quint8* curr_block = curr_block_row_start;

        quint8* is_different = is_diff_row_start;

        for (int x = 0; x < full_blocks_x_; ++x)
        {
            *is_different = DiffPartialBlock(prev_block,
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
                DiffPartialBlock(prev_block,
                                 curr_block,
                                 bytes_per_row_,
                                 partial_column_width_ * kBytesPerPixel,
                                 partial_row_height_);
        }
    }
}

//
// After the dirty blocks have been identified, this routine merges adjacent
// blocks into a region.
// The goal is to minimize the region that covers the dirty blocks.
//
void Differ::mergeBlocks(QRegion* dirty_region)
{
    quint8* is_diff_row_start = diff_info_.get();
    const int diff_stride = diff_width_;

    for (int y = 0; y < diff_height_; ++y)
    {
        quint8* is_different = is_diff_row_start;

        for (int x = 0; x < diff_width_; ++x)
        {
            // We've found a modified block. Look at blocks to the right and
            // below to group this block with as many others as we can.
            if (*is_different != 0)
            {
                // Width and height of the rectangle in blocks.
                int width  = 1;
                int height = 1;

                *is_different = 0;

                // Group with blocks to the right.
                // We can keep looking until we find an unchanged block because
                // we have a boundary block which is never marked as having
                // diffs.
                quint8* right = is_different + 1;

                while (*right != 0)
                {
                    *right++ = 0;
                    ++width;
                }

                // Group with blocks below.
                // The entire width of blocks that we matched above much match
                // for each row that we add.
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

                        // We need to go back and erase the diff markers so
                        // that we don't try to add these blocks a second time.
                        right = bottom;

                        for (int x2 = 0; x2 < width; ++x2)
                        {
                            *right++ = 0;
                        }
                    }
                } while (found_new_row);

                QRect dirty_rect(x * kBlockSize, y * kBlockSize,
                           width * kBlockSize, height * kBlockSize);

                // Add rect to region.
                *dirty_region += dirty_rect.intersected(screen_rect_);
            }

            // Increment to next block in this row.
            ++is_different;
        }

        // Go to start of next row.
        is_diff_row_start += diff_stride;
    }
}

void Differ::calcDirtyRegion(const quint8* prev_image,
                             const quint8* curr_image,
                             QRegion* dirty_region)
{
    *dirty_region = QRegion();

    // Identify all the blocks that contain changed pixels.
    markDirtyBlocks(prev_image, curr_image);

    //
    // Now that we've identified the blocks that have changed, merge adjacent
    // blocks to minimize the number of rects that we return.
    //
    mergeBlocks(dirty_region);
}

} // namespace aspia
