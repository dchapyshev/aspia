//
// PROJECT:         Aspia
// FILE:            report/table.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "report/table.h"
#include "report/report.h"
#include "system_info/category.h"
#include "base/logging.h"

namespace aspia {

Table::Table(Report* report, Category* category)
    : report_(report)
{
    DCHECK(report_);
    DCHECK(category);
    report_->StartTable(category);
}

Table::Table(Table&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
}

Table::~Table()
{
    if (report_)
        report_->EndTable();
}


Table& Table::operator=(Table&& other)
{
    report_ = other.report_;
    other.report_ = nullptr;
    return *this;
}

// static
Table Table::Create(Report* report, Category* category)
{
    return Table(report, category);
}

void Table::AddColumns(const ColumnList& column_list)
{
    DCHECK(report_);
    report_->AddColumns(column_list);
}

Group Table::AddGroup(std::string_view name)
{
    return Group(report_, name);
}

void Table::AddParam(std::string_view param, const Value& value)
{
    report_->AddParam(param, value);
}

Row Table::AddRow()
{
    return Row(report_);
}

} // namespace aspia
