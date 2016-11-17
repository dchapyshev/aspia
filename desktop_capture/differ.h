/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/differ.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__DIFFER_H
#define _ASPIA_DESKTOP_CAPTURE__DIFFER_H

#include "aspia_config.h"

#include <memory>

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_region_win.h"
#include "base/macros.h"

typedef uint8_t(*DIFFFULLBLOCK) (const uint8_t *image1, const uint8_t *image2, int bytes_per_row);

//
// Класс для поиска изменившихся областей экрана
//
class Differ
{
public:
    Differ(const DesktopSize &size, int bytes_per_pixel);
    ~Differ() {}

    // Размер блока
    static const int kBlockSize = 16;

    void CalcChangedRegion(const uint8_t *prev_image,
                           const uint8_t *curr_image,
                           DesktopRegion &changed_region);

private:
    void MarkChangedBlocks(const uint8_t *prev_image, const uint8_t *curr_image);
    void MergeChangedBlocks(DesktopRegion &changed_region);

private:
    DesktopSize size_;

    int bytes_per_pixel_;
    int bytes_per_block_;
    int bytes_per_row_;

    int full_blocks_x_;
    int full_blocks_y_;

    int partial_column_width_;
    int partial_row_height_;

    int block_stride_y_;

    int diff_width_;
    int diff_height_;

    DIFFFULLBLOCK DiffFullBlock_;

    std::unique_ptr<uint8_t[]> diff_info_;

    DISALLOW_COPY_AND_ASSIGN(Differ);
};

#endif // _ASPIA_DESKTOP_CAPTURE___DIFFER_H
