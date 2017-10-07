//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_display.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_display.h"

namespace aspia {

//
// Windows Video
//

class CategoryWindowsVideo : public Category
{
public:
    CategoryWindowsVideo();
    ~CategoryWindowsVideo() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryWindowsVideo);
};

CategoryWindowsVideo::CategoryWindowsVideo()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_DISPLAY_WINDOWS_VIDEO, IDI_MONITOR)
{
    set_guid(system_info::hardware::display::kWindowsVideo);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Monitor
//

class CategoryMonitor : public Category
{
public:
    CategoryMonitor();
    ~CategoryMonitor() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryMonitor);
};

CategoryMonitor::CategoryMonitor()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_DISPLAY_MONITOR, IDI_MONITOR)
{
    set_guid(system_info::hardware::display::kMonitor);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// OpenGL
//

class CategoryOpenGL : public Category
{
public:
    CategoryOpenGL();
    ~CategoryOpenGL() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenGL);
};

CategoryOpenGL::CategoryOpenGL()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_DISPLAY_OPENGL, IDI_CLAPPERBOARD)
{
    set_guid(system_info::hardware::display::kOpenGL);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Display Group
//

CategoryGroupDisplay::CategoryGroupDisplay()
    : CategoryGroup(IDS_SI_CATEGORY_DISPLAY, IDI_MONITOR)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryWindowsVideo>());
    child_list->emplace_back(std::make_unique<CategoryMonitor>());
    child_list->emplace_back(std::make_unique<CategoryOpenGL>());
}

} // namespace aspia
