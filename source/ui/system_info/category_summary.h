//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_summary.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H

#include "ui/system_info/category_info.h"

namespace aspia {

class CategorySummary : public CategoryInfo
{
public:
    CategorySummary();
    ~CategorySummary() = default;

    // CategoryInfo implementation.
    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySummary);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_SUMMARY_H
