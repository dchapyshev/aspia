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
#include <iterator>

//--------------------------------------------------------------------------------------------------
Region& Region::operator=(const QRect& rect)
{
    rows_.clear();
    cache_.clear();
    dirty_ = false;
    addRect(rect);
    return *this;
}

//--------------------------------------------------------------------------------------------------
Region Region::intersected(const QRect& rect) const
{
    Region clip(rect);
    Region result;
    result.intersect(*this, clip);
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

    for (auto it = rows_.upper_bound(top); it != rows_.end() && it->second.top < bottom; ++it)
    {
        for (const RowSpan& span : it->second.spans)
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

    dirty_ = true;

    if (dy == 0)
    {
        for (auto& entry : rows_)
        {
            for (RowSpan& span : entry.second.spans)
            {
                span.left += dx;
                span.right += dx;
            }
        }
        return;
    }

    // The row key is its bottom edge, which changes with |dy|, so the map has to be rebuilt.
    Rows shifted;
    for (auto& entry : rows_)
    {
        Row row = std::move(entry.second);
        row.top += dy;
        row.bottom += dy;

        if (dx != 0)
        {
            for (RowSpan& span : row.spans)
            {
                span.left += dx;
                span.right += dx;
            }
        }

        const int key = row.bottom;
        shifted.emplace(key, std::move(row));
    }

    rows_.swap(shifted);
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

    dirty_ = true;

    // Top of the part of the rectangle that has not been inserted yet. It is raised as we walk down
    // through the rows until it reaches |bottom|.
    int y = top;
    Rows::iterator row = rows_.upper_bound(y);

    while (y < bottom)
    {
        if (row == rows_.end() || y < row->second.top)
        {
            // |y| is above the current row: insert a new row above it.
            int row_bottom = bottom;
            if (row != rows_.end() && row->second.top < row_bottom)
                row_bottom = row->second.top;

            row = rows_.emplace_hint(row, row_bottom, Row{ y, row_bottom, {} });
        }
        else if (y > row->second.top)
        {
            // |y| falls in the middle of the row: split it, leaving |row| as the lower half ready to
            // receive the new span.
            rows_.emplace_hint(row, y, Row{ row->second.top, y, row->second.spans });
            row->second.top = y;
        }

        if (bottom < row->second.bottom)
        {
            // The bottom of the rectangle falls in the middle of the row: split it and continue with
            // the upper half.
            Rows::iterator lower = rows_.emplace_hint(row, bottom, Row{ y, bottom, row->second.spans });
            row->second.top = bottom;
            row = lower;
        }

        addSpanToRow(row->second, left, right);
        y = row->second.bottom;

        mergeWithPrecedingRow(row);
        ++row;
    }

    if (row != rows_.end())
        mergeWithPrecedingRow(row);
}

//--------------------------------------------------------------------------------------------------
void Region::addRegion(const Region& region)
{
    for (auto it = region.rows_.begin(); it != region.rows_.end(); ++it)
    {
        for (const RowSpan& span : it->second.spans)
            addRect(span.left, it->second.top, span.right, it->second.bottom);
    }
}

//--------------------------------------------------------------------------------------------------
void Region::mergeWithPrecedingRow(Rows::iterator row)
{
    if (row == rows_.begin())
        return;

    // If the row directly above is adjacent and contains exactly the same spans, the two rows can be
    // collapsed into a single taller row.
    Rows::iterator previous = std::prev(row);
    if (previous->second.bottom == row->second.top && previous->second.spans == row->second.spans)
    {
        row->second.top = previous->second.top;
        rows_.erase(previous);
    }
}

//--------------------------------------------------------------------------------------------------
void Region::intersect(const Region& region1, const Region& region2)
{
    rows_.clear();
    cache_.clear();
    dirty_ = true;

    Rows::const_iterator it1 = region1.rows_.begin();
    Rows::const_iterator end1 = region1.rows_.end();
    Rows::const_iterator it2 = region2.rows_.begin();
    Rows::const_iterator end2 = region2.rows_.end();

    while (it1 != end1 && it2 != end2)
    {
        // Arrange for |it1| to always be the top-most of the two rows.
        if (it2->second.top < it1->second.top)
        {
            std::swap(it1, it2);
            std::swap(end1, end2);
        }

        // Skip |it1| if it does not intersect |it2| at all.
        if (it1->second.bottom <= it2->second.top)
        {
            ++it1;
            continue;
        }

        const int top = it2->second.top;
        const int bottom = std::min(it1->second.bottom, it2->second.bottom);

        Row row{ top, bottom, {} };
        intersectRows(it1->second.spans, it2->second.spans, row.spans);

        if (!row.spans.empty())
        {
            Rows::iterator inserted = rows_.emplace_hint(rows_.end(), bottom, std::move(row));
            mergeWithPrecedingRow(inserted);
        }

        if (it1->second.bottom == bottom)
            ++it1;
        if (it2->second.bottom == bottom)
            ++it2;
    }
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
    // Fast path for the common case of spans inserted left to right.
    if (row.spans.empty() || left > row.spans.back().right)
    {
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

//--------------------------------------------------------------------------------------------------
bool Region::spanInRow(const Row& row, const RowSpan& span)
{
    RowSpanSet::const_iterator it = std::lower_bound(
        row.spans.begin(), row.spans.end(), span.left,
        [](const RowSpan& s, int value) { return s.left < value; });
    return it != row.spans.end() && *it == span;
}

//--------------------------------------------------------------------------------------------------
void Region::materialize() const
{
    if (!dirty_)
        return;

    cache_.clear();

    // Walk the rows producing canonical rectangles, merging each span with the matching spans in the
    // contiguous rows below it so vertically adjacent identical spans become a single rectangle.
    for (Rows::const_iterator row = rows_.begin(); row != rows_.end(); ++row)
    {
        for (const RowSpan& span : row->second.spans)
        {
            Rows::const_iterator previous = (row == rows_.begin()) ? rows_.end() : std::prev(row);
            if (previous != rows_.end() && previous->second.bottom == row->second.top &&
                spanInRow(previous->second, span))
            {
                continue; // already emitted as part of a rectangle started in a row above
            }

            int bottom = row->second.bottom;
            Rows::const_iterator next = std::next(row);
            while (next != rows_.end() && next->second.top == bottom &&
                   spanInRow(next->second, span))
            {
                bottom = next->second.bottom;
                ++next;
            }

            cache_.emplace_back(span.left, row->second.top,
                                span.right - span.left, bottom - row->second.top);
        }
    }

    dirty_ = false;
}
