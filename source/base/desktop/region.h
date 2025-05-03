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

#ifndef BASE_DESKTOP_REGION_H
#define BASE_DESKTOP_REGION_H

#include "base/desktop/geometry.h"

extern "C" {
#include "third_party/x11region/x11region.h"
} // extern "C"

namespace base {

// Region represents a region of the screen or window.
//
// Internally each region is stored as a set of rows where each row contains one
// or more rectangles aligned vertically.
class Region
{
public:
    // Iterator that can be used to iterate over rectangles of a Region.
    // The region must not be mutated while the iterator is used.
    class Iterator
    {
    public:
        explicit Iterator(const Region& target);
        ~Iterator() = default;

        bool isAtEnd() const;
        void advance();

        Rect rect() const
        {
            const BoxRec& current = rects_[pos_];
            return Rect::makeLTRB(current.x1, current.y1, current.x2, current.y2);
        }

    private:
        const BoxRec* rects_;
        long count_;
        long pos_;
    };

    Region();
    explicit Region(const Rect& rect);
    Region(const Rect* rects, int count);
    Region(const Region& other);
    Region(Region&& other) noexcept;
    ~Region();

    Region& operator=(const Region& other);
    Region& operator=(Region&& other) noexcept;

    bool isEmpty() const;

    bool equals(const Region& region) const;

    // Reset the region to be empty.
    void clear();

    // Reset region to contain just |rect|.
    void setRect(const Rect& rect);

    // Adds specified rect(s) or region to the region.
    void addRect(const Rect& rect);
    void addRects(const Rect* rects, int count);
    void addRegion(const Region& region);

    // Finds intersection of two regions and stores them in the current region.
    void intersect(const Region& region1, const Region& region2);

    // Same as above but intersects content of the current region with |region|.
    void intersectWith(const Region& region);

    // Clips the region by the |rect|.
    void intersectWith(const Rect& rect);

    // Subtracts |region| from the current content of the region.
    void subtract(const Region& region);

    // Subtracts |rect| from the current content of the region.
    void subtract(const Rect& rect);

    // Adds (dx, dy) to the position of the region.
    void translate(qint32 dx, qint32 dy);

    void swap(Region* region);

private:
    RegionRec x11reg_;
};

} // namespace base

#endif // BASE_DESKTOP_REGION_H
