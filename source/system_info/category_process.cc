//
// PROJECT:         Aspia
// FILE:            system_info/category_process.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_enumerator.h"
#include "system_info/category_process.h"
#include "system_info/category_process.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryProcess::CategoryProcess()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryProcess::Name() const
{
    return "Processes";
}

Category::IconId CategoryProcess::Icon() const
{
    return IDI_SYSTEM_MONITOR;
}

const char* CategoryProcess::Guid() const
{
    return "{14BB101B-EE61-49E6-B5B9-874C4DBEA03C}";
}

void CategoryProcess::Parse(Table& table, const std::string& data)
{
    proto::Processes message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Process Name", 150)
                     .AddColumn("File Path", 200)
                     .AddColumn("Used Memory", 80)
                     .AddColumn("Used Swap", 80)
                     .AddColumn("Description", 150));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Processes::Item& item = message.item(index);

        Row row = table.AddRow();
        row.AddValue(Value::String(item.process_name()));
        row.AddValue(Value::String(item.file_path()));
        row.AddValue(Value::Number(item.used_memory() / 1024, "kB"));
        row.AddValue(Value::Number(item.used_swap() / 1024, "kB"));
        row.AddValue(Value::String(item.description()));
    }
}

std::string CategoryProcess::Serialize()
{
    proto::Processes message;

    for (ProcessEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Processes::Item* item = message.add_item();

        item->set_process_name(enumerator.GetProcessName());
        item->set_file_path(enumerator.GetFilePath());
        item->set_used_memory(enumerator.GetUsedMemory());
        item->set_used_swap(enumerator.GetUsedSwap());
        item->set_description(enumerator.GetFileDescription());
    }

    return message.SerializeAsString();
}

} // namespace aspia
