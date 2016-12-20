/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/scoped_hdc.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SCOPED_HDC_H
#define _ASPIA_BASE__SCOPED_HDC_H

#include "aspia_config.h"

#include "base/logging.h"

namespace aspia {

//
// Like ScopedHandle but for HDC.  Only use this on HDCs returned from GetDC.
//
class ScopedGetDC
{
public:
    explicit ScopedGetDC(HWND hwnd) :
        hwnd_(hwnd),
        hdc_(GetDC(hwnd))
    {
        if (hwnd_)
        {
            DCHECK(IsWindow(hwnd_));
            DCHECK(hdc_);
        }
        else
        {
            // If GetDC(NULL) returns NULL, something really bad has happened, like
            // GDI handle exhaustion.  In this case Chrome is going to behave badly no
            // matter what, so we may as well just force a crash now.
            CHECK(hdc_);
        }
    }

    ~ScopedGetDC()
    {
        if (hdc_) ReleaseDC(hwnd_, hdc_);
    }

    operator HDC() { return hdc_; }

private:
    HWND hwnd_;
    HDC hdc_;

    DISALLOW_COPY_AND_ASSIGN(ScopedGetDC);
};

//
// Like ScopedHandle but for HDC.  Only use this on HDCs returned from
// CreateCompatibleDC, CreateDC and CreateIC.
//
class ScopedCreateDC
{
public:
    ScopedCreateDC() : hdc_(nullptr)
    {
        // Nothing
    }

    explicit ScopedCreateDC(HDC h) : hdc_(h)
    {
        // Nothing
    }

    ~ScopedCreateDC()
    {
        Close();
    }

    HDC get()
    {
        return hdc_;
    }

    void set(HDC h)
    {
        Close();
        hdc_ = h;
    }

    operator HDC() { return hdc_; }

private:
    void Close()
    {
        if (hdc_) DeleteDC(hdc_);
    }

    HDC hdc_;

    DISALLOW_COPY_AND_ASSIGN(ScopedCreateDC);
};

} // namespace aspia

#endif // _ASPIA_BASE__SCOPED_HDC_H
