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

#include <map>
#include <utility>
#include <vector>

// Region represents an area of the screen as a set of rectangles. It is API-compatible with the
// subset of QRegion used by the capture/codec pipeline, but is tuned for that workload. Internally
// the area is kept in canonical banded form: a set of rows (horizontal bands), each holding one or
// more non-overlapping horizontal spans. The rows are stored in a std::map keyed by their bottom
// edge so they remain ordered and adjacent operations can find the affected rows in logarithmic
// time. The flat list of rectangles used for iteration is built on demand and cached.
class Region
{
public:
    using const_iterator = std::vector<QRect>::const_iterator;

    Region() = default;
    Region(const QRect& rect) { addRect(rect); } // implicit, like QRegion(QRect)
    Region(const Region& other) = default;
    Region(Region&& other) noexcept = default;
    Region& operator=(const Region& other) = default;
    Region& operator=(Region&& other) noexcept = default;

    Region& operator=(const QRect& rect);

    bool isEmpty() const { return rows_.empty(); }

    void swap(Region& other) noexcept
    {
        rows_.swap(other.rows_);
        cache_.swap(other.cache_);
        std::swap(dirty_, other.dirty_);
    }

    Region& operator+=(const QRect& rect) { addRect(rect); return *this; }
    Region& operator+=(const Region& region) { addRegion(region); return *this; }

    Region intersected(const QRect& rect) const;
    Region intersected(const Region& region) const;

    bool contains(const QRect& rect) const;

    void translate(int dx, int dy);

    const_iterator begin() const { materialize(); return cache_.begin(); }
    const_iterator end() const { materialize(); return cache_.end(); }

private:
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

    using RowSpanSet = std::vector<RowSpan>;

    // Row is a horizontal band [top, bottom) together with the set of spans it contains.
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
    void materialize() const;

    static void intersectRows(const RowSpanSet& set1, const RowSpanSet& set2, RowSpanSet& output);
    static void addSpanToRow(Row& row, int left, int right);
    static bool spanInRow(const Row& row, const RowSpan& span);

    Rows rows_;
    mutable std::vector<QRect> cache_; // flat rectangles for iteration, valid when |dirty_| is false
    mutable bool dirty_ = false;
};

#endif // BASE_DESKTOP_REGION_H
