//
// PROJECT:         Aspia
// FILE:            system_info/category_open_files.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_open_files.h"
#include "system_info/category_open_files.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryOpenFiles::CategoryOpenFiles()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryOpenFiles::Name() const
{
    return "Open Files";
}

Category::IconId CategoryOpenFiles::Icon() const
{
    return IDI_FOLDER_NETWORK;
}

const char* CategoryOpenFiles::Guid() const
{
    return "{EAE638B9-CCF6-442C-84A1-B0901A64DA3D}";
}

void CategoryOpenFiles::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryOpenFiles::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
