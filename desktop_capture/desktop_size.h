/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_size.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H

#include "aspia_config.h"

#include <stdint.h>

namespace aspia {

class DesktopSize
{
public:
    DesktopSize();
    DesktopSize(int32_t width, int32_t height);
    DesktopSize(const DesktopSize &size);
    ~DesktopSize();

    int32_t width() const;
    int32_t height() const;

    void set_width(int32_t width);
    void set_height(int32_t height);

    bool IsEmpty() const;

    bool IsEqualTo(const DesktopSize &other) const;

    DesktopSize& operator=(const DesktopSize &size);

    bool operator==(const DesktopSize &size);

    bool operator!=(const DesktopSize &size);

private:
    int32_t width_;
    int32_t height_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
