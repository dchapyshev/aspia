//
// PROJECT:         Aspia
// FILE:            system_info/category_windows_device.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_enumerator.h"
#include "system_info/category_windows_device.h"
#include "system_info/category_windows_device.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryWindowsDevice::CategoryWindowsDevice()
    : CategoryInfo(Type::INFO_LIST)
{
    // Nothing
}

const char* CategoryWindowsDevice::Name() const
{
    return "Windows Devices";
}

Category::IconId CategoryWindowsDevice::Icon() const
{
    return IDI_PCI;
}

const char* CategoryWindowsDevice::Guid() const
{
    return "{22C4F1A6-67F2-4445-B807-9D39E1A80636}";
}

void CategoryWindowsDevice::Parse(Table& table, const std::string& data)
{
    proto::WindowsDevice message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Device Name", 200)
                     .AddColumn("Driver Version", 90)
                     .AddColumn("Driver Date", 80)
                     .AddColumn("Driver Vendor", 90)
                     .AddColumn("Device ID", 200));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::WindowsDevice::Item& item = message.item(index);

        Row row = table.AddRow();

        if (!item.friendly_name().empty())
            row.AddValue(Value::String(item.friendly_name()));
        else if (!item.description().empty())
            row.AddValue(Value::String(item.description()));
        else
            row.AddValue(Value::String("Unknown Device"));

        row.AddValue(Value::String(item.driver_version()));
        row.AddValue(Value::String(item.driver_date()));
        row.AddValue(Value::String(item.driver_vendor()));
        row.AddValue(Value::String(item.device_id()));
    }
}

std::string CategoryWindowsDevice::Serialize()
{
    proto::WindowsDevice message;

    for (DeviceEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::WindowsDevice::Item* item = message.add_item();

        item->set_friendly_name(enumerator.GetFriendlyName());
        item->set_description(enumerator.GetDescription());
        item->set_driver_version(enumerator.GetDriverVersion());
        item->set_driver_date(enumerator.GetDriverDate());
        item->set_driver_vendor(enumerator.GetDriverVendor());
        item->set_device_id(enumerator.GetDeviceID());
    }

    return message.SerializeAsString();
}

} // namespace aspia
