//
// PROJECT:         Aspia
// FILE:            ui/system_info/row.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/row.h"
#include "ui/system_info/output.h"
#include "base/logging.h"

namespace aspia {

Row::Row(Output* output)
    : output_(output)
{
    DCHECK(output_);
    output_->StartRow();
}

Row::Row(Row&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
}

Row::~Row()
{
    if (output_)
        output_->EndRow();
}

Row& Row::operator=(Row&& other)
{
    output_ = other.output_;
    other.output_ = nullptr;
    return *this;
}

void Row::AddValue(const Value& value)
{
    DCHECK(output_);
    output_->AddValue(value);
}

} // namespace aspia
