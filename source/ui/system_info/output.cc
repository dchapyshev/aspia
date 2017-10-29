//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output_proxy.h"

namespace aspia {

Output::Output()
{
    proxy_.reset(new OutputProxy(this));
}

Output::~Output()
{
    proxy_->WillDestroyCurrentOutput();
    proxy_ = nullptr;
}

Output::Table::Table(std::shared_ptr<OutputProxy> output, const std::string& name)
    : output_(std::move(output))
{
    output_->StartTable(name);
}

Output::Table::~Table()
{
    output_->EndTable();
}

Output::TableHeader::TableHeader(std::shared_ptr<OutputProxy> output)
    : output_(std::move(output))
{
    output_->StartTableHeader();
}

Output::TableHeader::~TableHeader()
{
    output_->EndTableHeader();
}

Output::Group::Group(std::shared_ptr<OutputProxy> output,
                     const std::string& name,
                     Category::IconId icon_id)
    : output_(std::move(output))
{
    output_->StartGroup(name, icon_id);
}

Output::Group::~Group()
{
    output_->EndGroup();
}

Output::Row::Row(std::shared_ptr<OutputProxy> output, Category::IconId icon_id)
    : output_(std::move(output))
{
    output_->StartRow(icon_id);
}

Output::Row::~Row()
{
    output_->EndRow();
}

} // namespace aspia
