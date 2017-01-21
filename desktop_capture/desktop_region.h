//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_region.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

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
    DesktopRegion(const DesktopRegion &other);
    ~DesktopRegion();

    void CopyFrom(const DesktopRegion &other);
    void Clear();
    bool IsEmpty() const;
    bool Equals(const DesktopRegion &other) const;
    void AddRegion(const DesktopRegion &other);
    void AddRect(const DesktopRect &rect);
    void Translate(int32_t x_offset, int32_t y_offset);
    void IntersectWith(const DesktopRegion &other);
    void IntersectWith(const DesktopRect &rect);

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
    mutable RegionRec rgn_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_REGION_H
