//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/region.h"

#include "base/compiler_specific.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
ALWAYS_INLINE RegionPtr region_cast(const RegionRec* region)
{
    return reinterpret_cast<RegionPtr>(const_cast<RegionRec*>(region));
}

} // namespace

//--------------------------------------------------------------------------------------------------
Region::Region()
{
    miRegionInit(&x11reg_, NullBox, 0);
}

//--------------------------------------------------------------------------------------------------
Region::Region(const Rect& rect)
{
    if (!rect.isEmpty())
    {
        BoxRec box;
        box.x1 = static_cast<short>(rect.left());
        box.x2 = static_cast<short>(rect.right());
        box.y1 = static_cast<short>(rect.top());
        box.y2 = static_cast<short>(rect.bottom());
        miRegionInit(&x11reg_, &box, 0);
    }
    else
    {
        miRegionInit(&x11reg_, NullBox, 0);
    }
}

//--------------------------------------------------------------------------------------------------
Region::Region(const Rect* rects, int count)
{
    miRegionInit(&x11reg_, NullBox, 0);
    addRects(rects, count);
}

//--------------------------------------------------------------------------------------------------
Region::Region(const Region& other)
{
    miRegionInit(&x11reg_, NullBox, 0);
    miRegionCopy(&x11reg_, region_cast(&other.x11reg_));
}

//--------------------------------------------------------------------------------------------------
Region::Region(Region&& other) noexcept
{
    *this = std::move(other);
}

//--------------------------------------------------------------------------------------------------
Region::~Region()
{
    miRegionUninit(&x11reg_);
}

//--------------------------------------------------------------------------------------------------
Region& Region::operator=(const Region& other)
{
    if (this == &other)
        return *this;

    miRegionCopy(&x11reg_, region_cast(&other.x11reg_));
    return *this;
}

//--------------------------------------------------------------------------------------------------
Region& Region::operator=(Region&& other) noexcept
{
    if (this == &other)
        return *this;

    miRegionUninit(&x11reg_);

    x11reg_.extents = other.x11reg_.extents;
    x11reg_.data = other.x11reg_.data;

    miRegionInit(&other.x11reg_, NullBox, 0);

    return *this;
}

//--------------------------------------------------------------------------------------------------
bool Region::isEmpty() const
{
    return (miRegionNotEmpty(region_cast(&x11reg_)) == FALSE);
}

//--------------------------------------------------------------------------------------------------
bool Region::equals(const Region& region) const
{
    if (isEmpty() && region.isEmpty())
        return true;

    return (miRegionsEqual(region_cast(&x11reg_), region_cast(&region.x11reg_)) != FALSE);
}

//--------------------------------------------------------------------------------------------------
void Region::clear()
{
    miRegionEmpty(&x11reg_);
}

//--------------------------------------------------------------------------------------------------
void Region::setRect(const Rect& rect)
{
    clear();
    addRect(rect);
}

//--------------------------------------------------------------------------------------------------
void Region::addRect(const Rect& rect)
{
    if (!rect.isEmpty())
    {
        Region temp(rect);
        addRegion(temp);
    }
}

//--------------------------------------------------------------------------------------------------
void Region::addRects(const Rect* rects, int count)
{
    for (int i = 0; i < count; ++i)
        addRect(rects[i]);
}

//--------------------------------------------------------------------------------------------------
void Region::addRegion(const Region& region)
{
    miUnion(&x11reg_, &x11reg_, region_cast(&region.x11reg_));
}

//--------------------------------------------------------------------------------------------------
void Region::intersect(const Region& region1, const Region& region2)
{
    miIntersect(&x11reg_, region_cast(&region1.x11reg_), region_cast(&region2.x11reg_));
}

//--------------------------------------------------------------------------------------------------
void Region::intersectWith(const Region& region)
{
    miIntersect(&x11reg_, &x11reg_, region_cast(&region.x11reg_));
}

//--------------------------------------------------------------------------------------------------
void Region::intersectWith(const Rect& rect)
{
    Region region;
    region.addRect(rect);
    intersectWith(region);
}

//--------------------------------------------------------------------------------------------------
void Region::subtract(const Region& region)
{
    miSubtract(&x11reg_, &x11reg_, region_cast(&region.x11reg_));
}

//--------------------------------------------------------------------------------------------------
void Region::subtract(const Rect& rect)
{
    Region region;
    region.addRect(rect);
    subtract(region);
}

//--------------------------------------------------------------------------------------------------
void Region::translate(qint32 dx, qint32 dy)
{
    miTranslateRegion(&x11reg_, dx, dy);
}

//--------------------------------------------------------------------------------------------------
void Region::swap(Region* region)
{
    std::swap(x11reg_.extents, region->x11reg_.extents);
    std::swap(x11reg_.data, region->x11reg_.data);
}

//--------------------------------------------------------------------------------------------------
Region::Iterator::Iterator(const Region& region)
    : rects_(REGION_RECTS(&region.x11reg_)),
      count_(REGION_NUM_RECTS(&region.x11reg_)),
      pos_(0)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool Region::Iterator::isAtEnd() const
{
    return pos_ >= count_;
}

//--------------------------------------------------------------------------------------------------
void Region::Iterator::advance()
{
    ++pos_;
}

} // namespace base
