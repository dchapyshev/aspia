//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output.h"

namespace aspia {

Output::Table::Table(Output* output,
                     Category* category,
                     TableType table_type)
    : output_(output)
{
    output_->StartTable(category, table_type);
}

Output::Table::~Table()
{
    output_->EndTable();
}

Output::Group::Group(Output* output, std::string_view name)
    : output_(output)
{
    output_->StartGroup(name);
}

Output::Group::~Group()
{
    output_->EndGroup();
}

Output::Row::Row(Output* output)
    : output_(output)
{
    output_->StartRow();
}

Output::Row::~Row()
{
    output_->EndRow();
}

} // namespace aspia
