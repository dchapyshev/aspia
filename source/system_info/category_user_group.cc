//
// PROJECT:         Aspia
// FILE:            system_info/category_user_group.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/user_group_enumerator.h"
#include "system_info/category_user_group.h"
#include "system_info/category_user_group.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryUserGroup::CategoryUserGroup()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryUserGroup::Name() const
{
    return "User Groups";
}

Category::IconId CategoryUserGroup::Icon() const
{
    return IDI_USERS;
}

const char* CategoryUserGroup::Guid() const
{
    return "{B560FDED-5E88-4116-98A5-12462C07AC90}";
}

void CategoryUserGroup::Parse(Table& table, const std::string& data)
{
    proto::UserGroup message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Group Name", 250)
                     .AddColumn("Description", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::UserGroup::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(item.comment()));
    }
}

std::string CategoryUserGroup::Serialize()
{
    proto::UserGroup message;

    for (UserGroupEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::UserGroup::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_comment(enumerator.GetComment());
    }

    return message.SerializeAsString();
}

} // namespace aspia
