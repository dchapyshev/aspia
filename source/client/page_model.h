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

#ifndef CLIENT_PAGE_MODEL_H
#define CLIENT_PAGE_MODEL_H

#include <QtTypes>

// Which window of a list the user is looking at. The list itself lives on the router and is asked
// for a page at a time, so all that is kept here is the page size, the page the user is on and how
// many items the router said there are.
class PageModel
{
public:
    PageModel() = default;
    ~PageModel() = default;

    // Back to the first page of an empty list. The page size is kept.
    void clear();

    void setPageSize(qint64 page_size);
    qint64 pageSize() const { return page_size_; }

    // Items over the whole list, as the router reported them with the last page. Returns true when
    // the current page did not survive the new count. The caller has to fetch the page it was moved
    // to: what it is showing was fetched for a page that is not there any more.
    bool setTotalCount(qint64 total_count);
    qint64 totalCount() const { return total_count_; }

    // At least one, even for an empty list: the user is always standing on a page.
    qint64 pageCount() const;

    // Zero based. Out of range values are clamped to the last page.
    qint64 currentPage() const { return current_page_; }
    void setCurrentPage(qint64 page);

    // Index of the first item of the current page. Together with pageSize() this is the window to
    // ask the router for. A short last page is not asked for by its real length: the router returns
    // what it has, and the length is not known before the answer anyway.
    qint64 offset() const { return current_page_ * page_size_; }

private:
    qint64 total_count_ = 0;
    qint64 page_size_ = 50;
    qint64 current_page_ = 0;
};

#endif // CLIENT_PAGE_MODEL_H
