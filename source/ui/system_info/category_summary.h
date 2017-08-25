//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_summary.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H

#include "ui/system_info/category.h"

namespace aspia {

class CategorySummary : public Category
{
public:
    CategorySummary();
    ~CategorySummary() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySummary);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H
