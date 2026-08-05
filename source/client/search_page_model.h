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

#ifndef CLIENT_SEARCH_PAGE_MODEL_H
#define CLIENT_SEARCH_PAGE_MODEL_H

#include <QList>
#include <QtTypes>

// Pagination of a host search whose matches come from several sources at once: the local address
// book and every router the client is connected to. Each source counts and orders its own matches
// and can hand out an arbitrary window of them, but none of them knows about the others, so the
// paging over the whole result belongs here.
//
// The order of the whole result is the sources in the order they were added, and inside a source
// the order that source produced. Sorting the result as a whole would mean merging the sources
// item by item, which needs a cursor into each of them on every page instead of a plain window,
// so the sources stay whole and follow one another.
//
// No UI and no networking: the model turns the match counts into the windows to ask each source
// for, and the widget only issues the requests and shows what comes back.
class SearchPageModel
{
public:
    SearchPageModel() = default;
    ~SearchPageModel() = default;

    // The window of one source that a page falls on.
    struct Slice
    {
        int source = -1;  // Index of the source, as returned by addSource().
        qint64 offset = 0; // First match to take from that source.
        qint64 count = 0;  // Number of matches to take, never zero.

        bool operator==(const Slice& other) const = default;
    };

    // Forgets every source and returns to the first page. Called when a new query starts.
    void clear();

    // Registers a source that reported |match_count| matches for the current query and returns its
    // index. A source that failed to answer is simply not added: its matches are then missing from
    // the result, which is what the user is shown anyway, and the pages of the others stay whole.
    int addSource(qint64 match_count);

    void setPageSize(qint64 page_size);
    qint64 pageSize() const { return page_size_; }

    // Matches over every source.
    qint64 totalCount() const { return total_count_; }

    // At least one, even when nothing was found: the user is always on a page.
    qint64 pageCount() const;

    // Zero based. Out of range values are clamped to the last page, so a page that disappeared
    // under the user (a source lost its matches on a refetch) does not leave an empty view.
    qint64 currentPage() const { return current_page_; }
    void setCurrentPage(qint64 page);

    // The windows to ask the sources for to fill the current page, in the order the results are
    // to be shown. Empty when nothing was found.
    QList<Slice> currentSlices() const;

private:
    QList<qint64> source_counts_;
    qint64 total_count_ = 0;
    qint64 page_size_ = 50;
    qint64 current_page_ = 0;
};

#endif // CLIENT_SEARCH_PAGE_MODEL_H
