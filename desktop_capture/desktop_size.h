//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H

#include <stdint.h>

namespace aspia {

class DesktopSize
{
public:
    DesktopSize() = default;
    DesktopSize(int width, int height);
    DesktopSize(const DesktopSize& other);
    ~DesktopSize() = default;

    int Width() const;
    int Height() const;

    void Set(int width, int height);
    bool IsEmpty() const;
    bool IsEqual(const DesktopSize& other) const;
    void Clear();

    DesktopSize& operator=(const DesktopSize& other);

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
