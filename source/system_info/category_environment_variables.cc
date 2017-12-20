//
// PROJECT:         Aspia
// FILE:            system_info/category_environment_variables.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_environment_variables.h"
#include "system_info/category_environment_variables.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEnvironmentVariables::CategoryEnvironmentVariables()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryEnvironmentVariables::Name() const
{
    return "Environment Variables";
}

Category::IconId CategoryEnvironmentVariables::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryEnvironmentVariables::Guid() const
{
    return "{AAB8670A-3C90-4F75-A907-512ACBAD1BE6}";
}

void CategoryEnvironmentVariables::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryEnvironmentVariables::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
