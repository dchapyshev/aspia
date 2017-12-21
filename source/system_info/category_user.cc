//
// PROJECT:         Aspia
// FILE:            system_info/category_user.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/datetime.h"
#include "base/user_enumerator.h"
#include "system_info/category_user.h"
#include "system_info/category_user.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryUser::CategoryUser()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryUser::Name() const
{
    return "Users";
}

Category::IconId CategoryUser::Icon() const
{
    return IDI_USER;
}

const char* CategoryUser::Guid() const
{
    return "{838AD22A-82BB-47F2-9001-1CD9714ED298}";
}

void CategoryUser::Parse(Table& table, const std::string& data)
{
    proto::User message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::User::Item& item = message.item(index);

        Group group = table.AddGroup(item.name());

        if (!item.full_name().empty())
            group.AddParam("Full Name", Value::String(item.full_name()));

        if (!item.comment().empty())
            group.AddParam("Description", Value::String(item.comment()));

        group.AddParam("Disabled", Value::Bool(item.is_disabled()));
        group.AddParam("Password Can't Change", Value::Bool(item.is_password_cant_change()));
        group.AddParam("Password Expired", Value::Bool(item.is_password_expired()));
        group.AddParam("Don't Expire Password", Value::Bool(item.is_dont_expire_password()));
        group.AddParam("Lockout", Value::Bool(item.is_lockout()));
        group.AddParam("Last Logon", Value::String(TimeToString(item.last_logon_time())));
        group.AddParam("Number Logons", Value::Number(item.number_logons()));
        group.AddParam("Bad Password Count", Value::Number(item.bad_password_count()));
    }
}

std::string CategoryUser::Serialize()
{
    proto::User message;

    for (UserEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::User::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_full_name(enumerator.GetFullName());
        item->set_comment(enumerator.GetComment());
        item->set_is_disabled(enumerator.IsDisabled());
        item->set_is_password_cant_change(enumerator.IsPasswordCantChange());
        item->set_is_password_expired(enumerator.IsPasswordExpired());
        item->set_is_dont_expire_password(enumerator.IsDontExpirePassword());
        item->set_is_lockout(enumerator.IsLockout());
        item->set_number_logons(enumerator.GetNumberLogons());
        item->set_bad_password_count(enumerator.GetBadPasswordCount());
        item->set_last_logon_time(enumerator.GetLastLogonTime());
    }

    return message.SerializeAsString();
}

} // namespace aspia
