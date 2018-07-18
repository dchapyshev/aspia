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

#ifndef _ASPIA_BASE__WIN__SCOPED_HDC_H
#define _ASPIA_BASE__WIN__SCOPED_HDC_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from GetDC.
class ScopedGetDC
{
public:
    explicit ScopedGetDC(HWND hwnd)
        : hwnd_(hwnd),
          hdc_(GetDC(hwnd))
    {
        if (hwnd_)
        {
            Q_ASSERT(IsWindow(hwnd_));
            Q_ASSERT(hdc_);
        }
        else
        {
            // If GetDC(NULL) returns NULL, something really bad has happened, like
            // GDI handle exhaustion.  In this case Chrome is going to behave badly no
            // matter what, so we may as well just force a crash now.
            if (!hdc_)
            {
                qFatal("!hdc_");
            }
        }
    }

    ~ScopedGetDC()
    {
        if (hdc_)
            ReleaseDC(hwnd_, hdc_);
    }

    operator HDC() const { return hdc_; }

private:
    HWND hwnd_;
    HDC hdc_;

    Q_DISABLE_COPY(ScopedGetDC);
};

// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC, CreateDC and CreateIC.
class ScopedCreateDC
{
public:
    ScopedCreateDC() = default;

    explicit ScopedCreateDC(HDC h)
        : hdc_(h)
    {
        // Nothing
    }

    ~ScopedCreateDC() { close(); }

    HDC get() { return hdc_; }

    void reset(HDC h = nullptr)
    {
        close();
        hdc_ = h;
    }

    operator HDC() { return hdc_; }

private:
    void close()
    {
        if (hdc_)
            DeleteDC(hdc_);
    }

    HDC hdc_ = nullptr;

    Q_DISABLE_COPY(ScopedCreateDC)
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_HDC_H
