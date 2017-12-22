//
// PROJECT:         Aspia
// FILE:            category/category_printer.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "category/category_printer.h"
#include "category/category_printer.pb.h"
#include "ui/resource.h"

#include <winspool.h>

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
    const DWORD flags = PRINTER_ENUM_FAVORITE | PRINTER_ENUM_LOCAL | PRINTER_ENUM_NETWORK;
    DWORD bytes_needed = 0;
    DWORD count = 0;

    if (EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes_needed, &count) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        DLOG(ERROR) << "Unexpected return value: " << GetLastSystemErrorString();
        return std::string();
    }

    std::unique_ptr<uint8_t[]> printers_info_buffer = std::make_unique<uint8_t[]>(bytes_needed);

    if (!EnumPrintersW(flags, nullptr, 2, printers_info_buffer.get(), bytes_needed,
                       &bytes_needed, &count))
    {
        DLOG(ERROR) << "EnumPrintersW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    WCHAR default_printer_name[256] = { 0 };
    DWORD characters_count = ARRAYSIZE(default_printer_name);

    if (!GetDefaultPrinterW(&default_printer_name[0], &characters_count))
    {
        DLOG(ERROR) << "GetDefaultPrinterW() failed: " << GetLastSystemErrorString();
    }

    PPRINTER_INFO_2W printers_info =
        reinterpret_cast<PPRINTER_INFO_2W>(printers_info_buffer.get());

    proto::Printers message;

    for (DWORD i = 0; i < count; ++i)
    {
        proto::Printers::Item* item = message.add_item();
        PPRINTER_INFO_2W printer = &printers_info[i];

        bool is_default = (printer->pPrinterName != nullptr &&
                           wcscmp(printer->pPrinterName, default_printer_name) == 0);

        item->set_name(UTF8fromUNICODE(printer->pPrinterName));
        item->set_is_default(is_default);
        item->set_is_shared(printer->Attributes & PRINTER_ATTRIBUTE_SHARED);
        item->set_share_name(UTF8fromUNICODE(printer->pShareName));
        item->set_port_name(UTF8fromUNICODE(printer->pPortName));
        item->set_driver_name(UTF8fromUNICODE(printer->pDriverName));

        if (printer->pDevMode)
        {
            item->set_device_name(UTF8fromUNICODE(printer->pDevMode->dmDeviceName));
            item->set_paper_width(printer->pDevMode->dmPaperWidth / 10);
            item->set_paper_length(printer->pDevMode->dmPaperLength / 10);
            item->set_print_quality(printer->pDevMode->dmPrintQuality);

            switch (printer->pDevMode->dmOrientation)
            {
                case DMORIENT_PORTRAIT:
                    item->set_orientation(proto::Printers::Item::ORIENTATION_PORTRAIT);
                    break;

                case DMORIENT_LANDSCAPE:
                    item->set_orientation(proto::Printers::Item::ORIENTATION_LANDSCAPE);
                    break;

                default:
                    break;
            }
        }

        item->set_print_processor(UTF8fromUNICODE(printer->pPrintProcessor));
        item->set_data_type(UTF8fromUNICODE(printer->pDatatype));
        item->set_server_name(UTF8fromUNICODE(printer->pServerName));
        item->set_location(UTF8fromUNICODE(printer->pLocation));
        item->set_comment(UTF8fromUNICODE(printer->pComment));
        item->set_jobs_count(printer->cJobs);
    }

    return message.SerializeAsString();
}

} // namespace aspia
