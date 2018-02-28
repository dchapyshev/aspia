//
// PROJECT:         Aspia
// FILE:            report/row.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__ROW_H
#define _ASPIA_REPORT__ROW_H

#include "report/value.h"

namespace aspia {

class Report;
class Table;
class Group;

class Row
{
public:
    Row(Row&& other);
    ~Row();

    void AddValue(const Value& value);

    Row& operator=(Row&& other);

private:
    friend class Table;
    friend class Group;

    Row(Report* report);

    Report* report_;
};

} // namespace aspia

#endif // _ASPIA_REPORT__ROW_H
