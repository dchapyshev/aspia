//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H

#include "aspia_config.h"

#include <stdint.h>

#include "proto/video.pb.h"

namespace aspia {

class DesktopSize
{
public:
    DesktopSize();
    DesktopSize(int32_t width, int32_t height);
    DesktopSize(const DesktopSize &other);
    ~DesktopSize();

    int32_t Width() const;
    int32_t Height() const;

    void Set(int32_t width, int32_t height);

    bool IsEmpty() const;
    bool IsEqualTo(const DesktopSize &other) const;

    void Clear();

    void ToVideoSize(proto::VideoSize *size) const;
    void FromVideoSize(const proto::VideoSize &size);

    DesktopSize& operator=(const DesktopSize &other);
    bool operator==(const DesktopSize &other);
    bool operator!=(const DesktopSize &other);

private:
    int32_t width_;
    int32_t height_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
