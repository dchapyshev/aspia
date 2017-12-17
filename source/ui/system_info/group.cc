//
// PROJECT:         Aspia
// FILE:            ui/system_info/group.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/group.h"
#include "ui/system_info/output.h"

namespace aspia {

Group::Group(Output* output, std::string_view name)
    : output_(output)
{
    DCHECK(output_);
    output_->StartGroup(name);
}

Group::Group(Group&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
}

Group::~Group()
{
    if (output_)
        output_->EndGroup();
}

Group& Group::operator=(Group&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

Group Group::AddGroup(std::string_view name)
{
    return Group(output_, name);
}

void Group::AddParam(std::string_view param, const Value& value)
{
    DCHECK(output_);
    output_->AddParam(param, value);
}

Row Group::AddRow()
{
    return Row(output_);
}

} // namespace aspia
