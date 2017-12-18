//
// PROJECT:         Aspia
// FILE:            system_info/category_memory.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "system_info/category_memory.h"
#include "system_info/category_memory.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryMemory::CategoryMemory()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryMemory::Name() const
{
    return "Memory";
}

Category::IconId CategoryMemory::Icon() const
{
    return IDI_MEMORY;
}

const char* CategoryMemory::Guid() const
{
    return "{71AEB9A0-ED07-4D1A-8B52-415695F2F722}";
}

void CategoryMemory::Parse(Table& table, const std::string& data)
{
    proto::Memory message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    if (message.total_physical())
    {
        Group group = table.AddGroup("Physical Memory");

        group.AddParam("Total", Value::MemorySizeInBytes(message.total_physical()));

        uint64_t used = message.total_physical() - message.free_physical();
        group.AddParam("Used", Value::MemorySizeInBytes(used));

        group.AddParam("Free", Value::MemorySizeInBytes(message.free_physical()));

        uint64_t utilization = (message.free_physical() * 100ULL) / message.total_physical();
        group.AddParam("Utilization", Value::Number(utilization, "%"));
    }

    if (message.total_page_file())
    {
        Group group = table.AddGroup("Page File Memory");

        group.AddParam("Total", Value::MemorySizeInBytes(message.total_page_file()));

        uint64_t used = message.total_page_file() - message.free_page_file();
        group.AddParam("Used", Value::MemorySizeInBytes(used));

        group.AddParam("Free", Value::MemorySizeInBytes(message.free_page_file()));

        uint64_t utilization = (message.free_page_file() * 100ULL) / message.total_page_file();
        group.AddParam("Utilization", Value::Number(utilization, "%"));
    }

    if (message.total_virtual())
    {
        Group group = table.AddGroup("Virtual Memory");

        group.AddParam("Total", Value::MemorySizeInBytes(message.total_virtual()));

        uint64_t used = message.total_virtual() - message.free_virtual();
        group.AddParam("Used", Value::MemorySizeInBytes(used));

        group.AddParam("Free", Value::MemorySizeInBytes(message.free_virtual()));

        uint64_t utilization = (message.free_virtual() * 100ULL) / message.total_virtual();
        group.AddParam("Utilization", Value::Number(utilization, "%"));
    }
}

std::string CategoryMemory::Serialize()
{
    proto::Memory message;

    MEMORYSTATUSEX memory_status;
    memset(&memory_status, 0, sizeof(memory_status));

    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status))
    {
        message.set_total_physical(memory_status.ullTotalPhys);
        message.set_free_physical(memory_status.ullAvailPhys);
        message.set_total_page_file(memory_status.ullTotalPageFile);
        message.set_free_page_file(memory_status.ullAvailPageFile);
        message.set_total_virtual(memory_status.ullTotalVirtual);
        message.set_free_virtual(memory_status.ullAvailVirtual);
    }

    return message.SerializeAsString();
}

} // namespace aspia
