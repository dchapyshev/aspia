//
// PROJECT:         Aspia
// FILE:            category/category_user.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/datetime.h"
#include "base/logging.h"
#include "category/category_user.h"
#include "category/category_user.pb.h"
#include "ui/resource.h"

#include <lm.h>

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
    PUSER_INFO_3 user_info = nullptr;
    DWORD total_entries = 0;
    DWORD entries_read = 0;

    DWORD error_code = NetUserEnum(nullptr, 3,
                                   FILTER_NORMAL_ACCOUNT,
                                   reinterpret_cast<LPBYTE*>(&user_info),
                                   MAX_PREFERRED_LENGTH,
                                   &entries_read,
                                   &total_entries,
                                   nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(WARNING) << "NetUserEnum() failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    proto::User message;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        proto::User::Item* item = message.add_item();

        item->set_name(UTF8fromUNICODE(user_info[i].usri3_name));
        item->set_full_name(UTF8fromUNICODE(user_info[i].usri3_full_name));
        item->set_comment(UTF8fromUNICODE(user_info[i].usri3_comment));
        item->set_is_disabled(user_info[i].usri3_flags & UF_ACCOUNTDISABLE);
        item->set_is_password_cant_change(user_info[i].usri3_flags & UF_PASSWD_CANT_CHANGE);
        item->set_is_password_expired(user_info[i].usri3_flags & UF_PASSWORD_EXPIRED);
        item->set_is_dont_expire_password(user_info[i].usri3_flags & UF_DONT_EXPIRE_PASSWD);
        item->set_is_lockout(user_info[i].usri3_flags & UF_LOCKOUT);
        item->set_number_logons(user_info[i].usri3_num_logons);
        item->set_bad_password_count(user_info[i].usri3_bad_pw_count);
        item->set_last_logon_time(user_info[i].usri3_last_logon);
    }

    NetApiBufferFree(user_info);

    return message.SerializeAsString();
}

} // namespace aspia
