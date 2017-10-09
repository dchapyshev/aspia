//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_display.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_display.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryWindowsVideo : public CategoryInfo
{
public:
    CategoryWindowsVideo()
        : CategoryInfo(system_info::hardware::display::kWindowsVideo,
                       IDS_SI_CATEGORY_DISPLAY_WINDOWS_VIDEO,
                       IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryWindowsVideo() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryWindowsVideo);
};

class CategoryMonitor : public CategoryInfo
{
public:
    CategoryMonitor()
        : CategoryInfo(system_info::hardware::display::kMonitor,
                       IDS_SI_CATEGORY_DISPLAY_MONITOR,
                       IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryMonitor() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryMonitor);
};

class CategoryOpenGL : public CategoryInfo
{
public:
    CategoryOpenGL()
        : CategoryInfo(system_info::hardware::display::kOpenGL,
                       IDS_SI_CATEGORY_DISPLAY_OPENGL,
                       IDI_CLAPPERBOARD)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpenGL() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenGL);
};

CategoryGroupDisplay::CategoryGroupDisplay()
    : CategoryGroup(IDS_SI_CATEGORY_DISPLAY, IDI_MONITOR)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryWindowsVideo>());
    child_list->emplace_back(std::make_unique<CategoryMonitor>());
    child_list->emplace_back(std::make_unique<CategoryOpenGL>());
}

} // namespace aspia
