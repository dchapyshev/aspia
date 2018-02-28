//
// PROJECT:         Aspia
// FILE:            category/category_user_group.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_user_group.h"
#include "category/category_user_group.pb.h"
#include "ui/resource.h"

#include <lm.h>

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
    PLOCALGROUP_INFO_1 group_info = nullptr;
    DWORD total_entries = 0;
    DWORD entries_read = 0;

    DWORD error_code = NetLocalGroupEnum(nullptr, 1,
                                         reinterpret_cast<LPBYTE*>(&group_info),
                                         MAX_PREFERRED_LENGTH,
                                         &entries_read,
                                         &total_entries,
                                         nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(LS_WARNING) << "NetUserEnum failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    proto::UserGroup message;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        proto::UserGroup::Item* item = message.add_item();

        item->set_name(UTF8fromUNICODE(group_info[i].lgrpi1_name));
        item->set_comment(UTF8fromUNICODE(group_info[i].lgrpi1_comment));
    }

    NetApiBufferFree(group_info);

    return message.SerializeAsString();
}

} // namespace aspia
