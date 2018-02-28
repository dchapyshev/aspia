//
// PROJECT:         Aspia
// FILE:            report/group.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "report/group.h"
#include "report/report.h"

namespace aspia {

Group::Group(Report* report, std::string_view name)
    : report_(report)
{
    DCHECK(report_);
    report_->StartGroup(name);
}

Group::Group(Group&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
}

Group::~Group()
{
    if (report_)
        report_->EndGroup();
}

Group& Group::operator=(Group&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
    return *this;
}

Group Group::AddGroup(std::string_view name)
{
    return Group(report_, name);
}

void Group::AddParam(std::string_view param, const Value& value)
{
    DCHECK(report_);
    report_->AddParam(param, value);
}

Row Group::AddRow()
{
    return Row(report_);
}

} // namespace aspia
