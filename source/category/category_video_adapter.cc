//
// PROJECT:         Aspia
// FILE:            category/category_video_adapter.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/video_adapter_enumarator.h"
#include "category/category_video_adapter.h"
#include "category/category_video_adapter.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryVideoAdapter::CategoryVideoAdapter()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryVideoAdapter::Name() const
{
    return "Video Adapters";
}

Category::IconId CategoryVideoAdapter::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryVideoAdapter::Guid() const
{
    return "{09E9069D-C394-4CD7-8252-E5CF83B7674C}";
}

void CategoryVideoAdapter::Parse(Table& table, const std::string& data)
{
    proto::VideoAdapter message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::VideoAdapter::Item& item = message.item(index);

        Group group = table.AddGroup(item.description());

        if (!item.description().empty())
            group.AddParam("Description", Value::String(item.description()));

        if (!item.adapter_string().empty())
            group.AddParam("Adapter String", Value::String(item.adapter_string()));

        if (!item.bios_string().empty())
            group.AddParam("BIOS String", Value::String(item.bios_string()));

        if (!item.chip_type().empty())
            group.AddParam("Chip Type", Value::String(item.chip_type()));

        if (!item.dac_type().empty())
            group.AddParam("DAC Type", Value::String(item.dac_type()));

        if (item.memory_size())
            group.AddParam("Memory Size", Value::Number(item.memory_size(), "Bytes"));

        if (!item.driver_date().empty())
            group.AddParam("Driver Date", Value::String(item.driver_date()));

        if (!item.driver_version().empty())
            group.AddParam("Driver Version", Value::String(item.driver_version()));

        if (!item.driver_provider().empty())
            group.AddParam("Driver Provider", Value::String(item.driver_provider()));
    }
}

std::string CategoryVideoAdapter::Serialize()
{
    proto::VideoAdapter message;

    for (VideoAdapterEnumarator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::VideoAdapter::Item* item = message.add_item();

        item->set_description(enumerator.GetDescription());
        item->set_adapter_string(enumerator.GetAdapterString());
        item->set_bios_string(enumerator.GetBIOSString());
        item->set_chip_type(enumerator.GetChipString());
        item->set_dac_type(enumerator.GetDACType());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_provider(enumerator.GetDriverVendor());
        item->set_memory_size(enumerator.GetMemorySize());
    }

    return message.SerializeAsString();
}

} // namespace aspia
