//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/table.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/table.h"
#include "ui/system_info/output.h"
#include "protocol/category.h"
#include "base/logging.h"

namespace aspia {

Table::Table(Output* output, Category* category, Type type)
    : output_(output)
{
    DCHECK(output_);
    output_->StartTable(category, type);
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
Table Table::List(Output* output, Category* category)
{
    return Table(output, category, Type::LIST);
}

// static
Table Table::ParamValue(Output* output, Category* category)
{
    return Table(output, category, Type::PARAM_VALUE);
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
