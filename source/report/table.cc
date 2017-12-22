//
// PROJECT:         Aspia
// FILE:            report/table.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "report/table.h"
#include "report/output.h"
#include "system_info/category.h"
#include "base/logging.h"

namespace aspia {

Table::Table(Output* output, Category* category)
    : output_(output)
{
    DCHECK(output_);
    DCHECK(category);
    output_->StartTable(category);
}

Table::Table(Table&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
}

Table::~Table()
{
    if (output_)
        output_->EndTable();
}


Table& Table::operator=(Table&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

// static
Table Table::Create(Output* output, Category* category)
{
    return Table(output, category);
}

void Table::AddColumns(const ColumnList& column_list)
{
    DCHECK(output_);
    output_->AddColumns(column_list);
}

Group Table::AddGroup(std::string_view name)
{
    return Group(output_, name);
}

void Table::AddParam(std::string_view param, const Value& value)
{
    output_->AddParam(param, value);
}

Row Table::AddRow()
{
    return Row(output_);
}

} // namespace aspia
