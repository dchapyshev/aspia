//
// PROJECT:         Aspia
// FILE:            base/devices/video_adapter_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DEVICES__VIDEO_ADAPTER_ENUMERATOR_H
#define _ASPIA_BASE__DEVICES__VIDEO_ADAPTER_ENUMERATOR_H

#include "base/devices/device_enumerator.h"

namespace aspia {

class VideoAdapterEnumarator : public DeviceEnumerator
{
public:
    VideoAdapterEnumarator();

    std::string GetAdapterString() const;
    std::string GetBIOSString() const;
    std::string GetChipString() const;
    std::string GetDACType() const;
    uint64_t GetMemorySize() const;

private:
    DISALLOW_COPY_AND_ASSIGN(VideoAdapterEnumarator);
};

} // namespace aspia

#endif // _ASPIA_BASE__DEVICES__VIDEO_ADAPTER_ENUMERATOR_H
