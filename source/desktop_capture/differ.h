//
// PROJECT:         Aspia
// FILE:            desktop_capture/differ.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFFER_H
#define _ASPIA_DESKTOP_CAPTURE__DIFFER_H

#include <QRegion>
#include <memory>

#include "base/macros.h"

namespace aspia {

// Class to search for changed regions of the screen.
class Differ
{
public:
    explicit Differ(const QSize& size);
    ~Differ() = default;

    void CalcDirtyRegion(const uint8_t* prev_image,
                         const uint8_t* curr_image,
                         QRegion* changed_region);

private:
    void MarkDirtyBlocks(const uint8_t* prev_image, const uint8_t* curr_image);
    void MergeBlocks(QRegion* dirty_region);

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

    DISALLOW_COPY_AND_ASSIGN(Differ);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE___DIFFER_H
