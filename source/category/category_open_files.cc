//
// PROJECT:         Aspia
// FILE:            category/category_open_files.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_open_files.h"
#include "category/category_open_files.pb.h"
#include "ui/resource.h"

#include <lm.h>

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

void CategoryOpenFiles::Parse(Table& table, const std::string& data)
{
    proto::OpenFile message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("ID", 100)
                     .AddColumn("User Name", 100)
                     .AddColumn("Locks Count", 100)
                     .AddColumn("File Path", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::OpenFile::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::Number(item.id()));
        row.AddValue(Value::String(item.user_name()));
        row.AddValue(Value::Number(item.lock_count()));
        row.AddValue(Value::String(item.file_path()));
    }
}

std::string CategoryOpenFiles::Serialize()
{
    PFILE_INFO_3 file_info;

    DWORD total_entries = 0;
    DWORD entries_read = 0;

    DWORD error_code = NetFileEnum(nullptr, nullptr, nullptr, 3,
                                   reinterpret_cast<LPBYTE*>(&file_info),
                                   MAX_PREFERRED_LENGTH,
                                   &entries_read,
                                   &total_entries,
                                   nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(LS_WARNING) << "NetShareEnum() failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    proto::OpenFile message;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        proto::OpenFile::Item* item = message.add_item();

        item->set_id(file_info[i].fi3_id);
        item->set_user_name(UTF8fromUNICODE(file_info[i].fi3_username));
        item->set_lock_count(file_info[i].fi3_num_locks);
        item->set_file_path(UTF8fromUNICODE(file_info[i].fi3_pathname));
    }

    NetApiBufferFree(file_info);

    return message.SerializeAsString();
}

} // namespace aspia
