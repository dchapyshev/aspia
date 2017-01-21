//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_region.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_region.h"

namespace aspia {

DesktopRegion::DesktopRegion()
{
    miRegionInit(&rgn_, NullBox, 0);
}

DesktopRegion::DesktopRegion(const DesktopRect &rect)
{
    if (!rect.IsEmpty())
    {
        BoxRec box;

        box.x1 = rect.Left();
        box.x2 = rect.Right();
        box.y1 = rect.Top();
        box.y2 = rect.Bottom();

        miRegionInit(&rgn_, &box, 0);
    }
    else
    {
        miRegionInit(&rgn_, NullBox, 0);
    }
}

DesktopRegion::DesktopRegion(const DesktopRegion &other)
{
    miRegionInit(&rgn_, NullBox, 0);
    miRegionCopy(&rgn_, &other.rgn_);
}

DesktopRegion::~DesktopRegion()
{
    miRegionUninit(&rgn_);
}

void DesktopRegion::CopyFrom(const DesktopRegion &other)
{
    miRegionCopy(&rgn_, &other.rgn_);
}

void DesktopRegion::Clear()
{
    miRegionEmpty(&rgn_);
}

bool DesktopRegion::IsEmpty() const
{
    return !miRegionNotEmpty(&rgn_);
}

bool DesktopRegion::Equals(const DesktopRegion &other) const
{
    if (IsEmpty() && other.IsEmpty())
        return true;

    return !!miRegionsEqual(&rgn_, &other.rgn_);
}

void DesktopRegion::AddRegion(const DesktopRegion &other)
{
    miUnion(&rgn_, &rgn_, &other.rgn_);
}

void DesktopRegion::AddRect(const DesktopRect &rect)
{
    if (!rect.IsEmpty())
    {
        DesktopRegion temp(rect);
        AddRegion(temp);
    }
}

void DesktopRegion::Translate(int32_t x_offset, int32_t y_offset)
{
    miTranslateRegion(&rgn_, x_offset, y_offset);
}

void DesktopRegion::IntersectWith(const DesktopRegion &other)
{
    miIntersect(&rgn_, &rgn_, &other.rgn_);
}

void DesktopRegion::IntersectWith(const DesktopRect &rect)
{
    DesktopRegion temp(rect);
    IntersectWith(temp);
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
    list_  = REGION_RECTS(&target.rgn_);
    count_ = REGION_NUM_RECTS(&target.rgn_);
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
    const BoxRec *box = &list_[index_];

    return DesktopRect::MakeLTRB(box->x1, box->y1, box->x2, box->y2);
}

} // namespace aspia
