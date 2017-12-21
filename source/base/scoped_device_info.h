//
// PROJECT:         Aspia
// FILE:            base/scoped_device_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SCOPED_DEVICE_INFO_H
#define _ASPIA_BASE__SCOPED_DEVICE_INFO_H

#include "base/macros.h"

#include <setupapi.h>

namespace aspia {

class ScopedDeviceInfo
{
public:
    explicit ScopedDeviceInfo(HDEVINFO device_info)
        : device_info_(device_info)
    {
        // Nothing
    }

    ~ScopedDeviceInfo()
    {
        Close();
    }

    HDEVINFO Get() const
    {
        return device_info_;
    }

    operator HDEVINFO()
    {
        return device_info_;
    }

    void Reset(HDEVINFO device_info = INVALID_HANDLE_VALUE)
    {
        Close();
        device_info_ = device_info;
    }

    HDEVINFO Release()
    {
        HDEVINFO device_info = device_info_;
        device_info_ = INVALID_HANDLE_VALUE;
        return device_info;
    }

    bool IsValid() const
    {
        return device_info_ != INVALID_HANDLE_VALUE;
    }

private:
    void Close()
    {
        if (IsValid())
        {
            SetupDiDestroyDeviceInfoList(device_info_);
            device_info_ = INVALID_HANDLE_VALUE;
        }
    }

    HDEVINFO device_info_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDeviceInfo);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_DEVICE_INFO_H
