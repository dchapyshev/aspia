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
#include <map>

class RegionIterator;

// Region represents an area of the screen as a set of rectangles. It is API-compatible with the
// subset of QRegion used by the capture/codec pipeline, but is tuned for that workload. Internally
// the area is kept in canonical banded form: a set of rows (horizontal bands), each holding one or
// more non-overlapping horizontal spans. The rows are stored in a std::map keyed by their bottom
// edge so they remain ordered and the rows affected by an operation can be found in logarithmic
// time. Rectangles for iteration are produced on the fly, without materializing an intermediate
// list.
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

    void swap(Region& other) noexcept { rows_.swap(other.rows_); }

    Region& operator+=(const QRect& rect) { addRect(rect); return *this; }
    Region& operator+=(const Region& region) { addRegion(region); return *this; }

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

    // Inline storage for a single span: the vast majority of rows contain exactly one span, so this
    // avoids a heap allocation per row while staying a drop-in for std::vector.
    using RowSpanSet = absl::InlinedVector<RowSpan, 1>;

    // Row is a horizontal band [top, bottom) together with the non-overlapping spans it contains.
    struct Row
    {
        int top;
        int bottom;
        RowSpanSet spans;
    };

    // Rows are keyed by their bottom edge so that std::map keeps them ordered by position.
    using Rows = std::map<int, Row>;

    void addRect(const QRect& rect);
    void addRect(int left, int top, int right, int bottom);
    void addRegion(const Region& region);
    void mergeWithPrecedingRow(Rows::iterator row);
    void intersect(const Region& region1, const Region& region2);

    static void intersectRows(const RowSpanSet& set1, const RowSpanSet& set2, RowSpanSet& output);
    static void addSpanToRow(Row& row, int left, int right);
    static bool spanInRow(const Row& row, const RowSpan& span);

    Rows rows_;
};

// Forward iterator over the canonical rectangles of a Region. The current rectangle is computed
// while walking the rows: a span that continues unchanged into the contiguous rows below it is
// merged into a single taller rectangle, so no intermediate list of rectangles is built.
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

    bool atEnd() const { return row_ == region_->rows_.end(); }
    void updateCurrent();
    void advance();

    const Region* region_;
    Region::Rows::const_iterator row_;
    Region::Rows::const_iterator previous_row_;
    Region::RowSpanSet::const_iterator span_;
    QRect current_;
};

#endif // BASE_DESKTOP_REGION_H
