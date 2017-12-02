//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output.h"

namespace aspia {

Output::Table::Table(Output* output, std::string_view name)
    : output_(output)
{
    output_->StartTable(name);
}

Output::Table::~Table()
{
    output_->EndTable();
}

Output::TableHeader::TableHeader(Output* output)
    : output_(output)
{
    output_->StartTableHeader();
}

Output::TableHeader::~TableHeader()
{
    output_->EndTableHeader();
}

Output::Group::Group(Output* output, std::string_view name, Category::IconId icon_id)
    : output_(std::move(output))
{
    output_->StartGroup(name, icon_id);
}

Output::Group::~Group()
{
    output_->EndGroup();
}

Output::Row::Row(Output* output, Category::IconId icon_id)
    : output_(output)
{
    output_->StartRow(icon_id);
}

Output::Row::~Row()
{
    output_->EndRow();
}

} // namespace aspia
