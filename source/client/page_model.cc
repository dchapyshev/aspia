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

#include "client/page_model.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
void PageModel::clear()
{
    total_count_ = 0;
    current_page_ = 0;
}

//--------------------------------------------------------------------------------------------------
void PageModel::setPageSize(qint64 page_size)
{
    CHECK_GT(page_size, 0);

    page_size_ = page_size;
    setCurrentPage(current_page_);
}

//--------------------------------------------------------------------------------------------------
bool PageModel::setTotalCount(qint64 total_count)
{
    CHECK_GE(total_count, 0);

    total_count_ = total_count;

    const qint64 previous_page = current_page_;
    setCurrentPage(current_page_);

    return current_page_ != previous_page;
}

//--------------------------------------------------------------------------------------------------
qint64 PageModel::pageCount() const
{
    if (total_count_ <= 0)
        return 1;

    return (total_count_ + page_size_ - 1) / page_size_;
}

//--------------------------------------------------------------------------------------------------
void PageModel::setCurrentPage(qint64 page)
{
    current_page_ = qBound(qint64(0), page, pageCount() - 1);
}
