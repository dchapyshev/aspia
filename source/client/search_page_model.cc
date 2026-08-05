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

#include "client/search_page_model.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
void SearchPageModel::clear()
{
    source_counts_.clear();
    page_.clear();
}

//--------------------------------------------------------------------------------------------------
int SearchPageModel::addSource(qint64 match_count)
{
    CHECK_GE(match_count, 0);

    source_counts_.append(match_count);
    page_.setTotalCount(page_.totalCount() + match_count);

    return static_cast<int>(source_counts_.size()) - 1;
}

//--------------------------------------------------------------------------------------------------
QList<SearchPageModel::Slice> SearchPageModel::currentSlices() const
{
    QList<Slice> slices;

    const qint64 window_start = page_.offset();
    const qint64 window_end = window_start + page_.pageSize();

    // Walk the sources in order, tracking where each of them starts in the whole result, and take
    // the part of it the window covers. A source before or after the window contributes nothing.
    qint64 source_start = 0;

    for (int i = 0; i < source_counts_.size(); ++i)
    {
        const qint64 source_count = source_counts_[i];
        const qint64 source_end = source_start + source_count;

        const qint64 from = qMax(window_start, source_start);
        const qint64 to = qMin(window_end, source_end);

        if (to > from)
        {
            Slice slice;
            slice.source = i;
            slice.offset = from - source_start;
            slice.count = to - from;
            slices.append(slice);
        }

        source_start = source_end;

        if (source_start >= window_end)
            break;
    }

    return slices;
}
