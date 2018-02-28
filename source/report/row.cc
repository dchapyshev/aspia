//
// PROJECT:         Aspia
// FILE:            report/row.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "report/row.h"
#include "report/report.h"
#include "base/logging.h"

namespace aspia {

Row::Row(Report* report)
    : report_(report)
{
    DCHECK(report_);
    report_->StartRow();
}

Row::Row(Row&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
}

Row::~Row()
{
    if (report_)
        report_->EndRow();
}

Row& Row::operator=(Row&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
    return *this;
}

void Row::AddValue(const Value& value)
{
    DCHECK(report_);
    report_->AddValue(value);
}

} // namespace aspia
