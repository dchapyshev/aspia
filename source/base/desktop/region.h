//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QRect>

#include <absl/container/inlined_vector.h>

#include <iterator>
#include <vector>

class RegionIterator;

// Region represents an area of the screen as a set of rectangles. It is API-compatible with the
// subset of QRegion used by the capture/codec pipeline, but is tuned for that workload. Internally
// the area is kept in canonical banded form: a flat, position-sorted vector of rows (horizontal
// bands), each holding one or more non-overlapping horizontal spans. The flat vector keeps the rows
// in contiguous memory (cache-friendly iteration, one allocation that grows amortized and is reused
// across operations), and the single span of the typical row is stored inline. Rectangles for
// iteration are produced on the fly, without materializing an intermediate list.
class Region
{
public:
    Region() = default;
    Region(const QRect& rect) { addRect(rect); } // implicit, like QRegion(QRect)
    Region(const Region& other) = default;
    Region(Region&& other) noexcept = default;
    Region& operator=(const Region& other) = default;
    Region& operator=(Region&& other) noexcept = default;
    Region& operator=(const QRect& rect);

    bool isEmpty() const { return rows_.empty(); }

    // Empties the region but keeps the allocated capacity, so a region object reused across frames
    // (reset then rebuilt) does not reallocate its row buffer every time.
    void clear() { rows_.clear(); }

    void swap(Region& other) noexcept { rows_.swap(other.rows_); }

    Region& operator+=(const QRect& rect) { addRect(rect); return *this; }

    Region& operator+=(const Region& region)
    {
        if (this == &region)
            return *this; // union with self leaves a canonical region unchanged
        if (rows_.empty())
            *this = region; // copying is cheaper than re-adding every span
        else
            addRegion(region);
        return *this;
    }

    // Clips this region to |rect| in place. Cheaper than |region = region.intersected(rect)| because
    // it prunes the existing rows instead of building and assigning a new region - in particular a
    // clamp that changes nothing does almost no work.
    void intersect(const QRect& rect);

    Region intersected(const QRect& rect) const;
    Region intersected(const Region& region) const;

    bool contains(const QRect& rect) const;

    void translate(int dx, int dy);

    RegionIterator begin() const;
    RegionIterator end() const;

private:
    friend class RegionIterator;

    // RowSpan is a half-open horizontal span [left, right) within a single row.
    struct RowSpan
    {
        RowSpan(int left, int right) : left(left), right(right) {}

        bool operator==(const RowSpan& other) const
        {
            return left == other.left && right == other.right;
        }

        int left;
        int right;
    };

    // The single span of the typical row is stored inline to avoid a per-row heap allocation.
    using RowSpanSet = absl::InlinedVector<RowSpan, 1>;

    // Row is a horizontal band [top, bottom) together with the non-overlapping spans it contains.
    struct Row
    {
        int top;
        int bottom;
        RowSpanSet spans;
    };

    // Rows are kept sorted by position (top/bottom ascending; the bands never overlap vertically).
    using RowList = std::vector<Row>;

    void addRect(const QRect& rect);
    void addRect(int left, int top, int right, int bottom);
    void addRegion(const Region& region);
    bool mergeWithPrecedingRow(size_t index);
    void intersect(const Region& region1, const Region& region2);
    size_t firstRowBelow(int y) const;

    static void intersectRows(const RowSpanSet& set1, const RowSpanSet& set2, RowSpanSet& output);
    static void addSpanToRow(Row& row, int left, int right);

    RowList rows_;
};

// Forward iterator over the canonical rectangles of a Region. It yields one rectangle per (row,
// span): the bands are already merged at build time wherever whole rows share the same spans, so
// this matches the banded decomposition QRegion produces.
class RegionIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = QRect;
    using difference_type = std::ptrdiff_t;
    using pointer = const QRect*;
    using reference = const QRect&;

    const QRect& operator*() const { return current_; }
    const QRect* operator->() const { return &current_; }

    RegionIterator& operator++() { advance(); return *this; }
    RegionIterator operator++(int) { RegionIterator copy(*this); advance(); return copy; }

    bool operator==(const RegionIterator& other) const;
    bool operator!=(const RegionIterator& other) const { return !(*this == other); }

private:
    friend class Region;

    enum AtEndTag { AT_END };

    explicit RegionIterator(const Region* region);
    RegionIterator(const Region* region, AtEndTag);

    bool atEnd() const { return row_ >= region_->rows_.size(); }
    void updateCurrent();
    void advance();

    const Region* region_;
    size_t row_;
    size_t span_;
    QRect current_;
};

#endif // BASE_DESKTOP_REGION_H
