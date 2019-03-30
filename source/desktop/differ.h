//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__DIFFER_H
#define DESKTOP__DIFFER_H

#include "base/macros_magic.h"

#include <QRegion>

#include <memory>

namespace desktop {

// Class to search for changed regions of the screen.
class Differ
{
public:
    explicit Differ(const QSize& size);
    ~Differ() = default;

    void calcDirtyRegion(const uint8_t* prev_image,
                         const uint8_t* curr_image,
                         QRegion* changed_region);

private:
    void markDirtyBlocks(const uint8_t* prev_image, const uint8_t* curr_image);
    void mergeBlocks(QRegion* dirty_region);

    const QRect screen_rect_;

    const int bytes_per_row_;

    const int full_blocks_x_;
    const int full_blocks_y_;

    int partial_column_width_;
    int partial_row_height_;

    int block_stride_y_;

    const int diff_width_;
    const int diff_height_;

    std::unique_ptr<uint8_t[]> diff_info_;

    typedef uint8_t(*DiffFullBlockFunc)(const uint8_t*, const uint8_t*, int);
    DiffFullBlockFunc diff_full_block_func_;

    DISALLOW_COPY_AND_ASSIGN(Differ);
};

} // namespace desktop

#endif // DESKTOP___DIFFER_H
