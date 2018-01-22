//
// PROJECT:         Aspia
// FILE:            category/category_share.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_share.h"
#include "category/category_share.pb.h"
#include "ui/resource.h"

#include <lm.h>

namespace aspia {

namespace {

const char* TypeToString(proto::Share::Type type)
{
    switch (type)
    {
        case proto::Share::TYPE_DISK:
            return "Disk";

        case proto::Share::TYPE_PRINTER:
            return "Printer";

        case proto::Share::TYPE_DEVICE:
            return "Device";

        case proto::Share::TYPE_IPC:
            return "IPC";

        case proto::Share::TYPE_SPECIAL:
            return "Special";

        case proto::Share::TYPE_TEMPORARY:
            return "Temporary";

        default:
            return "Unknown";
    }
}

} // namespace

CategoryShare::CategoryShare()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryShare::Name() const
{
    return "Shared Resources";
}

Category::IconId CategoryShare::Icon() const
{
    return IDI_FOLDER_NETWORK;
}

const char* CategoryShare::Guid() const
{
    return "{9219D538-E1B8-453C-9298-61D5B80C4130}";
}

void CategoryShare::Parse(Table& table, const std::string& data)
{
    proto::Share message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Name", 120)
                     .AddColumn("Type", 70)
                     .AddColumn("Description", 100)
                     .AddColumn("Local Path", 150)
                     .AddColumn("Current Uses", 100)
                     .AddColumn("Maximum Uses", 100));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Share::Item& item = message.item(index);

        Row row = table.AddRow();

        row.AddValue(Value::String(item.name()));
        row.AddValue(Value::String(TypeToString(item.type())));
        row.AddValue(Value::String(item.description()));
        row.AddValue(Value::String(item.local_path()));
        row.AddValue(Value::Number(item.current_uses()));
        row.AddValue(Value::Number(item.maximum_uses()));
    }
}

std::string CategoryShare::Serialize()
{
    PSHARE_INFO_502 share_info;

    DWORD total_entries = 0;
    DWORD entries_read = 0;

    DWORD error_code = NetShareEnum(nullptr, 502,
                                    reinterpret_cast<LPBYTE*>(&share_info),
                                    MAX_PREFERRED_LENGTH,
                                    &entries_read,
                                    &total_entries,
                                    nullptr);
    if (error_code != NERR_Success)
    {
        DLOG(LS_WARNING) << "NetShareEnum() failed: " << SystemErrorCodeToString(error_code);
        return std::string();
    }

    proto::Share message;

    for (DWORD i = 0; i < total_entries; ++i)
    {
        proto::Share::Item* item = message.add_item();

        item->set_name(UTF8fromUNICODE(share_info[i].shi502_netname));

        switch (share_info[i].shi502_type)
        {
            case STYPE_DISKTREE:
                item->set_type(proto::Share::TYPE_DISK);
                break;

            case STYPE_PRINTQ:
                item->set_type(proto::Share::TYPE_PRINTER);
                break;

            case STYPE_DEVICE:
                item->set_type(proto::Share::TYPE_DEVICE);
                break;

            case STYPE_IPC:
                item->set_type(proto::Share::TYPE_IPC);
                break;

            case STYPE_SPECIAL:
                item->set_type(proto::Share::TYPE_SPECIAL);
                break;

            case STYPE_TEMPORARY:
                item->set_type(proto::Share::TYPE_TEMPORARY);
                break;

            default:
                break;
        }

        item->set_local_path(UTF8fromUNICODE(share_info[i].shi502_path));
        item->set_current_uses(share_info[i].shi502_current_uses);
        item->set_maximum_uses(share_info[i].shi502_max_uses);
    }

    NetApiBufferFree(share_info);

    return message.SerializeAsString();
}

} // namespace aspia
