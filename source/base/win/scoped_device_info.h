//
// PROJECT:         Aspia
// FILE:            base/win/scoped_device_info.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_DEVICE_INFO_H
#define _ASPIA_BASE__WIN__SCOPED_DEVICE_INFO_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>

namespace aspia {

class ScopedDeviceInfo
{
public:
    ScopedDeviceInfo() = default;

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

    HDEVINFO device_info_ = INVALID_HANDLE_VALUE;

    Q_DISABLE_COPY(ScopedDeviceInfo)
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_DEVICE_INFO_H
