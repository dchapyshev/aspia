//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_summary.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/category_summary.h"
#include "ui/resource.h"

namespace aspia {

const char* CategorySummary::Name() const
{
    return "Summary";
}

Category::IconId CategorySummary::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategorySummary::Guid() const
{
    return "8A66762A-0B23-47CB-B66E-A371B3B7111A";
}

void CategorySummary::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    // TODO
}

std::string CategorySummary::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
