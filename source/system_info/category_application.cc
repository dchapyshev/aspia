//
// PROJECT:         Aspia
// FILE:            system_info/category_application.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/program_enumerator.h"
#include "system_info/category_application.h"
#include "system_info/category_application.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryApplication::CategoryApplication()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryApplication::Name() const
{
    return "Applications";
}

Category::IconId CategoryApplication::Icon() const
{
    return IDI_APPLICATIONS;
}

const char* CategoryApplication::Guid() const
{
    return "{606C70BE-0C6C-4CB6-90E6-D374760FC5EE}";
}

void CategoryApplication::Parse(Table& table, const std::string& data)
{
    proto::Application message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Name", 200)
                     .AddColumn("Version", 100)
                     .AddColumn("Publisher", 100)
                     .AddColumn("Install Date", 80)
                     .AddColumn("Install Location", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Application::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.version()));
        row.AddValue(Value::String(item.publisher()));
        row.AddValue(Value::String(item.install_date()));
        row.AddValue(Value::String(item.install_location()));
    }
}

std::string CategoryApplication::Serialize()
{
    proto::Application message;

    for (ProgramEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Application::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_version(enumerator.GetVersion());
        item->set_publisher(enumerator.GetPublisher());
        item->set_install_date(enumerator.GetInstallDate());
        item->set_install_location(enumerator.GetInstallLocation());
    }

    return message.SerializeAsString();
}

} // namespace aspia
