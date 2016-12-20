/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_region.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/desktop_region.h"

namespace aspia {

DesktopRegion::DesktopRegion()
{
    miRegionInit(&region_, NullBox, 0);
}

DesktopRegion::DesktopRegion(const DesktopRect &rect)
{
    if (!rect.IsEmpty())
    {
        BoxRec box;

        box.x1 = rect.left();
        box.x2 = rect.right();
        box.y1 = rect.top();
        box.y2 = rect.bottom();

        miRegionInit(&region_, &box, 0);
    }
    else
    {
        miRegionInit(&region_, NullBox, 0);
    }
}

DesktopRegion::DesktopRegion(const DesktopRegion &region)
{
    miRegionInit(&region_, NullBox, 0);
    miRegionCopy(&region_, (RegionPtr)(&region.region_));
}

DesktopRegion::~DesktopRegion()
{
    miRegionUninit(&region_);
}

void DesktopRegion::CopyFrom(const DesktopRegion &region)
{
    Clear();
    miRegionCopy(&region_, (RegionPtr)(&region.region_));
}

void DesktopRegion::Clear()
{
    miRegionEmpty(&region_);
}

bool DesktopRegion::IsEmpty() const
{
    return (miRegionNotEmpty((RegionPtr)&region_) == FALSE);
}

bool DesktopRegion::Equals(const DesktopRegion &region) const
{
    if (IsEmpty() && region.IsEmpty())
        return true;

    return !!miRegionsEqual((RegionPtr)&region_, (RegionPtr)&region.region_);
}

void DesktopRegion::AddRegion(const DesktopRegion &region)
{
    miUnion(&region_, &region_, (RegionPtr)&region.region_);
}

void DesktopRegion::AddRect(const DesktopRect &rect)
{
    if (!rect.IsEmpty())
    {
        DesktopRegion temp(rect);
        AddRegion(temp);
    }
}

DesktopRegion& DesktopRegion::operator=(const DesktopRegion &other)
{
    CopyFrom(other);
    return *this;
}

bool DesktopRegion::operator==(const DesktopRegion &other)
{
    return Equals(other);
}

bool DesktopRegion::operator!=(const DesktopRegion &other)
{
    return !Equals(other);
}

DesktopRegion::Iterator::Iterator(const DesktopRegion &target)
{
    Reset(target);
}

DesktopRegion::Iterator::~Iterator()
{
    // Nothing
}

void DesktopRegion::Iterator::Reset(const DesktopRegion &target)
{
    list_  = REGION_RECTS(&target.region_);
    count_ = REGION_NUM_RECTS(&target.region_);
    index_ = 0;
}

bool DesktopRegion::Iterator::IsAtEnd() const
{
    return index_ >= count_;
}

void DesktopRegion::Iterator::Advance()
{
    ++index_;
}

DesktopRect DesktopRegion::Iterator::rect()
{
    const BoxRec *r = &list_[index_];

    return DesktopRect::MakeLTRB(r->x1, r->y1, r->x2, r->y2);
}

} // namespace aspia
