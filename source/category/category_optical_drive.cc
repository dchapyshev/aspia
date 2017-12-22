//
// PROJECT:         Aspia
// FILE:            category/category_optical_drive.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_optical_drive.h"
#include "category/category_optical_drive.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryOpticalDrive::CategoryOpticalDrive()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryOpticalDrive::Name() const
{
    return "Optical Drives";
}

Category::IconId CategoryOpticalDrive::Icon() const
{
    return IDI_DRIVE_DISK;
}

const char* CategoryOpticalDrive::Guid() const
{
    return "{68E028FE-3DA6-4BAF-9E18-CDB828372860}";
}

void CategoryOpticalDrive::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryOpticalDrive::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
