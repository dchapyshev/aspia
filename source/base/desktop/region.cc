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

#include "base/desktop/region.h"

#include <algorithm>
#include <utility>

//--------------------------------------------------------------------------------------------------
RegionIterator::RegionIterator(const Region* region)
    : region_(region),
      row_(0),
      span_(0)
{
    if (!atEnd())
        updateCurrent();
}

//--------------------------------------------------------------------------------------------------
RegionIterator::RegionIterator(const Region* region, AtEndTag)
    : region_(region),
      row_(region->rows_.size()),
      span_(0)
{
    // |row_| at the end marks the past-the-end iterator.
}

//--------------------------------------------------------------------------------------------------
bool RegionIterator::operator==(const RegionIterator& other) const
{
    const bool at_end = atEnd();
    const bool other_at_end = other.atEnd();

    if (at_end || other_at_end)
        return at_end == other_at_end;

    return row_ == other.row_ && span_ == other.span_;
}

//--------------------------------------------------------------------------------------------------
void RegionIterator::updateCurrent()
{
    const Region::Row& row = region_->rows_[row_];
    const Region::RowSpan& span = row.spans[span_];
    current_ = QRect(span.left, row.top, span.right - span.left, row.bottom - row.top);
}

//--------------------------------------------------------------------------------------------------
void RegionIterator::advance()
{
    ++span_;
    if (span_ >= region_->rows_[row_].spans.size())
    {
        ++row_;
        span_ = 0;
        if (atEnd())
            return;
    }

    updateCurrent();
}

//--------------------------------------------------------------------------------------------------
RegionIterator Region::begin() const
{
    return RegionIterator(this);
}

//--------------------------------------------------------------------------------------------------
RegionIterator Region::end() const
{
    return RegionIterator(this, RegionIterator::AT_END);
}

//--------------------------------------------------------------------------------------------------
Region& Region::operator=(const QRect& rect)
{
    clear();
    addRect(rect);
    return *this;
}

//--------------------------------------------------------------------------------------------------
void Region::intersect(const QRect& rect)
{
    const int left = rect.x();
    const int top = rect.y();
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();

    if (right <= left || bottom <= top)
    {
        rows_.clear();
        return;
    }

    // Walk the rows in place: drop the ones outside the clip rectangle, clip the rest, and compact
    // the survivors to the front, coalescing newly-identical adjacent rows as we go.
    size_t write = 0;
    for (size_t read = 0; read < rows_.size(); ++read)
    {
        Row& row = rows_[read];
        if (row.bottom <= top || row.top >= bottom)
            continue;

        RowSpanSet& spans = row.spans;
        int kept = 0;
        for (int s = 0; s < static_cast<int>(spans.size()); ++s)
        {
            const int span_left = std::max(spans[s].left, left);
            const int span_right = std::min(spans[s].right, right);
            if (span_left < span_right)
                spans[kept++] = RowSpan(span_left, span_right);
        }
        spans.erase(spans.begin() + kept, spans.end());
        if (spans.empty())
            continue;

        row.top = std::max(row.top, top);
        row.bottom = std::min(row.bottom, bottom);

        if (write > 0 && rows_[write - 1].bottom == row.top && rows_[write - 1].spans == row.spans)
        {
            rows_[write - 1].bottom = row.bottom; // coalesce with the previous kept row
        }
        else
        {
            if (write != read)
                rows_[write] = std::move(row);
            ++write;
        }
    }

    rows_.erase(rows_.begin() + write, rows_.end());
}

//--------------------------------------------------------------------------------------------------
Region Region::intersected(const QRect& rect) const
{
    Region result(*this);
    result.intersect(rect);
    return result;
}

//--------------------------------------------------------------------------------------------------
Region Region::intersected(const Region& region) const
{
    Region result;
    result.intersect(*this, region);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool Region::contains(const QRect& rect) const
{
    // Matches QRegion::contains(const QRect&): true unless the rectangle lies completely outside the
    // region (any overlap counts, full containment is not required).
    const int left = rect.x();
    const int top = rect.y();
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();

    if (right <= left || bottom <= top)
        return false;

    for (size_t i = firstRowBelow(top); i < rows_.size() && rows_[i].top < bottom; ++i)
    {
        for (const RowSpan& span : rows_[i].spans)
        {
            if (span.left < right && left < span.right)
                return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Region::translate(int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;

    for (Row& row : rows_)
    {
        row.top += dy;
        row.bottom += dy;
        for (RowSpan& span : row.spans)
        {
            span.left += dx;
            span.right += dx;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Region::addRect(const QRect& rect)
{
    addRect(rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
}

//--------------------------------------------------------------------------------------------------
void Region::addRect(int left, int top, int right, int bottom)
{
    if (right <= left || bottom <= top)
        return;

    // Top of the part of the rectangle that has not been inserted yet, raised as we walk down.
    int y = top;

    // Fast path for the scanline/append pattern (differ, scaler): the rectangle starts in or below
    // the last row, so the binary search can be skipped.
    size_t i;
    if (rows_.empty() || y >= rows_.back().bottom)
        i = rows_.size();
    else if (y >= rows_.back().top)
        i = rows_.size() - 1;
    else
        i = firstRowBelow(y);

    while (y < bottom)
    {
        if (i >= rows_.size() || y < rows_[i].top)
        {
            // |y| is above the current row: insert a new row above it (or at the end).
            int row_bottom = bottom;
            if (i < rows_.size() && rows_[i].top < row_bottom)
                row_bottom = rows_[i].top;

            rows_.insert(rows_.begin() + i, Row{ y, row_bottom, {} });
        }
        else if (y > rows_[i].top)
        {
            // |y| falls in the middle of the row: split off the upper part, leaving |i| as the lower
            // half ready to receive the new span.
            RowSpanSet upper_spans = rows_[i].spans;
            const int upper_top = rows_[i].top;
            rows_[i].top = y;
            rows_.insert(rows_.begin() + i, Row{ upper_top, y, std::move(upper_spans) });
            ++i;
        }

        if (bottom < rows_[i].bottom)
        {
            // The bottom of the rectangle falls in the middle of the row: split it and continue with
            // the lower half.
            RowSpanSet lower_spans = rows_[i].spans;
            const int lower_top = rows_[i].top;
            rows_[i].top = bottom;
            rows_.insert(rows_.begin() + i, Row{ lower_top, bottom, std::move(lower_spans) });
        }

        addSpanToRow(rows_[i], left, right);
        y = rows_[i].bottom;

        if (!mergeWithPrecedingRow(i))
            ++i;
    }

    if (i < rows_.size())
        mergeWithPrecedingRow(i);
}

//--------------------------------------------------------------------------------------------------
void Region::addRegion(const Region& region)
{
    for (const Row& row : region.rows_)
    {
        for (const RowSpan& span : row.spans)
            addRect(span.left, row.top, span.right, row.bottom);
    }
}

//--------------------------------------------------------------------------------------------------
bool Region::mergeWithPrecedingRow(size_t index)
{
    if (index == 0 || index >= rows_.size())
        return false;

    // If the row directly above is adjacent and contains exactly the same spans, the two rows can be
    // collapsed into a single taller row.
    if (rows_[index - 1].bottom == rows_[index].top && rows_[index - 1].spans == rows_[index].spans)
    {
        rows_[index].top = rows_[index - 1].top;
        rows_.erase(rows_.begin() + (index - 1));
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Region::intersect(const Region& region1, const Region& region2)
{
    rows_.clear();

    RowList::const_iterator it1 = region1.rows_.begin();
    RowList::const_iterator end1 = region1.rows_.end();
    RowList::const_iterator it2 = region2.rows_.begin();
    RowList::const_iterator end2 = region2.rows_.end();

    while (it1 != end1 && it2 != end2)
    {
        // Arrange for |it1| to always be the top-most of the two rows.
        if (it2->top < it1->top)
        {
            std::swap(it1, it2);
            std::swap(end1, end2);
        }

        // Skip |it1| if it does not intersect |it2| at all.
        if (it1->bottom <= it2->top)
        {
            ++it1;
            continue;
        }

        const int top = it2->top;
        const int bottom = std::min(it1->bottom, it2->bottom);

        Row row{ top, bottom, {} };
        intersectRows(it1->spans, it2->spans, row.spans);

        if (!row.spans.empty())
        {
            rows_.push_back(std::move(row));
            mergeWithPrecedingRow(rows_.size() - 1);
        }

        if (it1->bottom == bottom)
            ++it1;
        if (it2->bottom == bottom)
            ++it2;
    }
}

//--------------------------------------------------------------------------------------------------
size_t Region::firstRowBelow(int y) const
{
    // First row whose bottom edge is below |y|, i.e. the first one that can contain or lie below it.
    return static_cast<size_t>(std::upper_bound(
        rows_.begin(), rows_.end(), y,
        [](int value, const Row& row) { return value < row.bottom; }) - rows_.begin());
}

//--------------------------------------------------------------------------------------------------
void Region::intersectRows(const RowSpanSet& set1, const RowSpanSet& set2, RowSpanSet& output)
{
    RowSpanSet::const_iterator it1 = set1.begin();
    RowSpanSet::const_iterator end1 = set1.end();
    RowSpanSet::const_iterator it2 = set2.begin();
    RowSpanSet::const_iterator end2 = set2.end();

    while (it1 != end1 && it2 != end2)
    {
        // Arrange for |it1| to always be the left-most of the two spans.
        if (it2->left < it1->left)
        {
            std::swap(it1, it2);
            std::swap(end1, end2);
        }

        if (it1->right <= it2->left)
        {
            ++it1;
            continue;
        }

        const int left = it2->left;
        const int right = std::min(it1->right, it2->right);
        output.emplace_back(left, right);

        if (it1->right == right)
            ++it1;
        if (it2->right == right)
            ++it2;
    }
}

//--------------------------------------------------------------------------------------------------
void Region::addSpanToRow(Row& row, int left, int right)
{
    // Fast path for the common case of spans appended left to right: extend the last span if the new
    // one touches it, otherwise append a new span.
    if (row.spans.empty() || left >= row.spans.back().right)
    {
        if (!row.spans.empty() && left == row.spans.back().right)
            row.spans.back().right = right;
        else
            row.spans.emplace_back(left, right);
        return;
    }

    // First span that ends at or after |left|.
    RowSpanSet::iterator start = std::lower_bound(
        row.spans.begin(), row.spans.end(), left,
        [](const RowSpan& span, int value) { return span.right < value; });

    // First span that starts after |right|.
    RowSpanSet::iterator finish = std::lower_bound(
        start, row.spans.end(), right + 1,
        [](const RowSpan& span, int value) { return span.left < value; });

    if (finish == row.spans.begin())
    {
        row.spans.emplace(row.spans.begin(), left, right);
        return;
    }

    --finish;

    if (finish < start)
    {
        // No overlap: insert the new span at its place.
        row.spans.emplace(start, left, right);
        return;
    }

    // Replace the overlapping range [start, finish] with the merged span.
    left = std::min(left, start->left);
    right = std::max(right, finish->right);

    *start = RowSpan{ left, right };
    ++start;
    ++finish;
    if (start < finish)
        row.spans.erase(start, finish);
}
