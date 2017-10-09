//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_summary.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_summary.h"
#include "ui/resource.h"

namespace aspia {

CategorySummary::CategorySummary()
    : CategoryInfo(system_info::kSummary,
                   IDS_SI_CATEGORY_SUMMARY,
                   IDI_COMPUTER)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

void CategorySummary::Parse(const std::string& data, Output* output)
{
    // TODO
}

} // namespace aspia
