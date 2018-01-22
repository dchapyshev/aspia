//
// PROJECT:         Aspia
// FILE:            category/category_environment_variables.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_environment_variables.h"
#include "category/category_environment_variables.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryEnvironmentVariables::CategoryEnvironmentVariables()
    : CategoryInfo(Type::INFO_LIST)
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

void CategoryEnvironmentVariables::Parse(Table& table, const std::string& data)
{
    proto::EnvironmentVariable message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Variable", 250)
                     .AddColumn("Value", 300));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::EnvironmentVariable::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.value()));
    }
}

std::string CategoryEnvironmentVariables::Serialize()
{
    WCHAR* strings = GetEnvironmentStringsW();
    if (!strings)
    {
        PLOG(LS_WARNING) << "GetEnvironmentStringsW() failed";
        return std::string();
    }

    proto::EnvironmentVariable message;

    for (WCHAR* current = strings; *current != 0; current++)
    {
        WCHAR* context;

        WCHAR* name = std::wcstok(current, L"=", &context);
        if (name != nullptr && std::wcsncmp(name, L"::", 2) != 0)
        {
            WCHAR* value = std::wcstok(nullptr, L"=", &context);
            if (value != nullptr)
            {
                proto::EnvironmentVariable::Item* item = message.add_item();

                item->set_name(UTF8fromUNICODE(name));
                item->set_value(UTF8fromUNICODE(value));
            }
        }

        while (*current != 0)
            current++;
    }

    FreeEnvironmentStringsW(strings);

    return message.SerializeAsString();
}

} // namespace aspia
