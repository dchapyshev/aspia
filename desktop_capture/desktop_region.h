/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_region.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_H

#include "desktop_capture/desktop_rect.h"

extern "C" {
#include "desktop_capture/x11region.h"
}

namespace aspia {

class DesktopRegion
{
public:
    DesktopRegion();
    explicit DesktopRegion(const DesktopRect &rect);
    DesktopRegion(const DesktopRegion &region);
    ~DesktopRegion();

    void CopyFrom(const DesktopRegion &region);
    void Clear();
    bool IsEmpty() const;
    bool Equals(const DesktopRegion &region) const;
    void AddRegion(const DesktopRegion &region);
    void AddRect(const DesktopRect &rect);

    class Iterator
    {
    public:
        explicit Iterator(const DesktopRegion &target);
        ~Iterator();

        void Reset(const DesktopRegion &target);

        bool IsAtEnd() const;
        void Advance();

        DesktopRect rect();

    private:
        const BoxRec *list_;
        DWORD index_;
        DWORD count_;
    };

    DesktopRegion& operator=(const DesktopRegion &other);
    bool operator==(const DesktopRegion &other);
    bool operator!=(const DesktopRegion &other);

private:
    RegionRec region_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_H
