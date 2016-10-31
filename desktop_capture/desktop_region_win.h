/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_region_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_WIN_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_WIN_H

#include "aspia_config.h"

#include "desktop_capture/desktop_rect.h"

class DesktopRegion
{
public:
    DesktopRegion();
    DesktopRegion(const DesktopRect &rect);
    DesktopRegion(const DesktopRegion &region);
    ~DesktopRegion();

    void CopyFrom(const DesktopRegion &region);
    void Clear();
    bool is_empty() const;
    bool Equals(const DesktopRegion &region) const;
    void AddRegion(const DesktopRegion &region);
    void AddRect(const DesktopRect &rect);

    class Iterator
    {
    public:
        explicit Iterator(const DesktopRegion &target);
        ~Iterator();

        bool IsAtEnd() const;
        void Advance();

        DesktopRect rect();

    private:
        PRGNDATA data_;
        DWORD index_;
        DWORD count_;
    };

    DesktopRegion& operator=(const DesktopRegion &other);
    bool operator==(const DesktopRegion &other);
    bool operator!=(const DesktopRegion &other);

private:
    HRGN region_;
};

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_WIN_H
