//
// PROJECT:         Aspia
// FILE:            report/column_list.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__COLUMN_LIST_H
#define _ASPIA_REPORT__COLUMN_LIST_H

#include "base/macros.h"

#include <string>
#include <vector>

namespace aspia {

class ColumnList
{
public:
    using Item = std::pair<std::string, int>;
    using List = std::vector<Item>;
    using ConstIterator = List::const_iterator;

    ColumnList(ColumnList&& other);
    ColumnList& operator=(ColumnList&& other);

    static ColumnList Create();

    ColumnList& AddColumn(std::string_view name, int width);

    ConstIterator begin() const;
    ConstIterator end() const;

private:
    ColumnList();

    List list_;

    DISALLOW_COPY_AND_ASSIGN(ColumnList);
};

} // namespace aspia

#endif // _ASPIA_REPORT__COLUMN_LIST_H
