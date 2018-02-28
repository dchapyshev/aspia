//
// PROJECT:         Aspia
// FILE:            report/group.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__GROUP_H
#define _ASPIA_REPORT__GROUP_H

#include "base/macros.h"
#include "report/value.h"
#include "report/row.h"

namespace aspia {

class Report;

class Group
{
public:
    Group(Group&& other);
    ~Group();

    Group AddGroup(std::string_view name);
    void AddParam(std::string_view param, const Value& value);
    Row AddRow();

    Group& operator=(Group&& other);

private:
    friend class Table;

    Group(Report* report, std::string_view name);

    Report* report_;

    DISALLOW_COPY_AND_ASSIGN(Group);
};

} // namespace aspia

#endif // _ASPIA_REPORT__GROUP_H
