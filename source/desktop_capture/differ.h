//
// PROJECT:         Aspia
// FILE:            desktop_capture/differ.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFFER_H
#define _ASPIA_DESKTOP_CAPTURE__DIFFER_H

#include <QRegion>
#include <memory>

namespace aspia {

// Class to search for changed regions of the screen.
class Differ
{
public:
    explicit Differ(const QSize& size);
    ~Differ() = default;

    void calcDirtyRegion(const quint8* prev_image,
                         const quint8* curr_image,
                         QRegion* changed_region);

private:
    void markDirtyBlocks(const quint8* prev_image, const quint8* curr_image);
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

    std::unique_ptr<quint8[]> diff_info_;

    Q_DISABLE_COPY(Differ)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE___DIFFER_H
