//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_summary.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "protocol/category_summary.h"
#include "ui/resource.h"

namespace aspia {

CategorySummary::CategorySummary() : CategoryInfo(system_info::kSummary, "Summary", IDI_COMPUTER)
{
    // Nothing
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
