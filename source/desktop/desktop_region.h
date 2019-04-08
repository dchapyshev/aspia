//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__DESKTOP_REGION_H
#define DESKTOP__DESKTOP_REGION_H

#include "desktop/desktop_geometry.h"

#if defined(USE_TBB)
#include <tbb/scalable_allocator.h>
#endif // defined(USE_TBB)

#include <map>
#include <vector>

namespace desktop {

// Region represents a region of the screen or window.
//
// Internally each region is stored as a set of rows where each row contains one
// or more rectangles aligned vertically.
class Region
{
private:
    // The following private types need to be declared first because they are used
    // in the public Iterator.

    // RowSpan represents a horizontal span withing a single row.
    struct RowSpan
    {
        RowSpan(int32_t left, int32_t right);

        // Used by std::vector<>.
        bool operator==(const RowSpan& that) const
        {
            return left == that.left && right == that.right;
        }

        int32_t left;
        int32_t right;
    };

#if defined(USE_TBB)
    using RowSpanSetAllocator = tbb::scalable_allocator<RowSpan>;
#else // defined(USE_TBB)
    using RowSpanSetAllocator = std::allocator<RowSpan>;
#endif // defined(USE_*)

    using RowSpanSet = std::vector<RowSpan, RowSpanSetAllocator>;

    // Row represents a single row of a region. A row is set of rectangles that
    // have the same vertical position.
    struct Row
    {
        Row(const Row&);
        Row(Row&&) noexcept;
        Row(int32_t top, int32_t bottom);
        ~Row();

        Row& operator=(const Row& other);

        int32_t top;
        int32_t bottom;

        RowSpanSet spans;
    };

    // Type used to store list of rows in the region. The bottom position of row
    // is used as the key so that rows are always ordered by their position. The
    // map stores pointers to make translate() more efficient.

#if defined(USE_TBB)
    using RowsAllocator = tbb::scalable_allocator<std::pair<const int, Row*>>;
#else // defined(USE_TBB)
    using RowsAllocator = std::allocator<std::pair<const int, Row*>>;
#endif // defined(USE_*)

    using Rows = std::map<const int, Row* , std::less<const int>, RowsAllocator>;

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

        const Rect& rect() const { return rect_; }

    private:
        const Region& region_;

        // Updates |rect_| based on the current |row_| and |row_span_|. If
        // |row_span_| matches spans on consecutive rows then they are also merged
        // into |rect_|, to generate more efficient output.
        void updateCurrentRect();

        Rows::const_iterator row_;
        Rows::const_iterator previous_row_;
        RowSpanSet::const_iterator row_span_;
        Rect rect_;
    };

    Region();
    explicit Region(const Rect& rect);
    Region(const Rect* rects, int count);
    Region(const Region& other);
    Region(Region&& other);
    ~Region();

    Region& operator=(const Region& other);
    Region& operator=(Region&& other);

    bool isEmpty() const { return rows_.empty(); }

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
    void translate(int32_t dx, int32_t dy);

    void swap(Region* region);

private:
    // Comparison functions used for std::lower_bound(). Compare left or right
    // edges withs a given |value|.
    static bool compareSpanLeft(const RowSpan& r, int32_t value);
    static bool compareSpanRight(const RowSpan& r, int32_t value);

    // Adds a new span to the row, coalescing spans if necessary.
    static void addSpanToRow(Row* row, int32_t left, int32_t right);

    // Returns true if the |span| exists in the given |row|.
    static bool isSpanInRow(const Row& row, const RowSpan& rect);

    // Calculates the intersection of two sets of spans.
    static void intersectRows(const RowSpanSet& set1, const RowSpanSet& set2, RowSpanSet& output);

    static void subtractRows(const RowSpanSet& set_a, const RowSpanSet& set_b, RowSpanSet& output);

    // Merges |row| with the row above it if they contain the same spans. Doesn't
    // do anything if called with |row| set to rows_.begin() (i.e. first row of
    // the region). If the rows were merged |row| remains a valid iterator to the
    // merged row.
    void mergeWithPrecedingRow(Rows::iterator row_it);

    Rows rows_;
};

} // namespace desktop

#endif // DESKTOP__DESKTOP_REGION_H
