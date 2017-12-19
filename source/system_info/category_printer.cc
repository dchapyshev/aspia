//
// PROJECT:         Aspia
// FILE:            system_info/category_printer.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "system_info/category_printer.h"
#include "system_info/category_printer.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryPrinter::CategoryPrinter()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryPrinter::Name() const
{
    return "Printers";
}

Category::IconId CategoryPrinter::Icon() const
{
    return IDI_PRINTER;
}

const char* CategoryPrinter::Guid() const
{
    return "{ACBDCE39-CE38-4A79-9626-8C8BA2E3A26A}";
}

void CategoryPrinter::Parse(Table& table, const std::string& data)
{
    proto::Printers message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 250)
                     .AddColumn("Value", 250));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::Printers::Item& item = message.item(index);

        Group group = table.AddGroup(item.name());

        group.AddParam("Default Printer", Value::Bool(item.is_default()));
        group.AddParam("Shared Printer", Value::Bool(item.is_shared()));
        group.AddParam("Port", Value::String(item.port_name()));
        group.AddParam("Driver", Value::String(item.driver_name()));
        group.AddParam("Device Name", Value::String(item.device_name()));
        group.AddParam("Print Processor", Value::String(item.print_processor()));
        group.AddParam("Data Type", Value::String(item.data_type()));
        group.AddParam("Print Jobs Queued", Value::Number(item.jobs_count()));

        if (item.paper_width())
        {
            group.AddParam("Paper Width", Value::Number(item.paper_width(), "mm"));
        }

        if (item.paper_length())
        {
            group.AddParam("Paper Length", Value::Number(item.paper_length(), "mm"));
        }

        if (item.print_quality())
        {
            group.AddParam("Print Quality", Value::Number(item.print_quality(), "dpi"));
        }

        switch (item.orientation())
        {
            case proto::Printers::Item::ORIENTATION_LANDSCAPE:
                group.AddParam("Orientation", Value::String("Landscape"));
                break;

            case proto::Printers::Item::ORIENTATION_PORTRAIT:
                group.AddParam("Orientation", Value::String("Portrait"));
                break;

            default:
                group.AddParam("Orientation", Value::String("Unknown"));
                break;
        }
    }
}

std::string CategoryPrinter::Serialize()
{
    proto::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::Printers::Item* item = message.add_item();

        item->set_name(enumerator.GetName());
        item->set_is_default(enumerator.IsDefault());
        item->set_is_shared(enumerator.IsShared());
        item->set_share_name(enumerator.GetShareName());
        item->set_port_name(enumerator.GetPortName());
        item->set_driver_name(enumerator.GetDriverName());
        item->set_device_name(enumerator.GetDeviceName());
        item->set_print_processor(enumerator.GetPrintProcessor());
        item->set_data_type(enumerator.GetDataType());
        item->set_server_name(enumerator.GetServerName());
        item->set_location(enumerator.GetLocation());
        item->set_comment(enumerator.GetComment());
        item->set_jobs_count(enumerator.GetJobsCount());
        item->set_paper_width(enumerator.GetPaperWidth());
        item->set_paper_length(enumerator.GetPaperLength());
        item->set_print_quality(enumerator.GetPrintQuality());

        switch (enumerator.GetOrientation())
        {
            case PrinterEnumerator::Orientation::PORTRAIT:
                item->set_orientation(proto::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(proto::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(proto::Printers::Item::ORIENTATION_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
