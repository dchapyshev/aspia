//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "protocol/category_group_hardware.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

//
// CategoryDmiBios
//

const char* CategoryDmiBios::Name() const
{
    return "BIOS";
}

Category::IconId CategoryDmiBios::Icon() const
{
    return IDI_BIOS;
}

const char* CategoryDmiBios::Guid() const
{
    return "B0B73D57-2CDC-4814-9AE0-C7AF7DDDD60E";
}

void CategoryDmiBios::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiBios::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiSystem
//

const char* CategoryDmiSystem::Name() const
{
    return "System";
}

Category::IconId CategoryDmiSystem::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategoryDmiSystem::Guid() const
{
    return "F599BBA4-AEBB-4583-A15E-9848F4C98601";
}

void CategoryDmiSystem::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiSystem::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiMotherboard
//

const char* CategoryDmiMotherboard::Name() const
{
    return "Motherboard";
}

Category::IconId CategoryDmiMotherboard::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiMotherboard::Guid() const
{
    return "8143642D-3248-40F5-8FCF-629C581FFF01";
}

void CategoryDmiMotherboard::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiMotherboard::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiChassis
//

const char* CategoryDmiChassis::Name() const
{
    return "Chassis";
}

Category::IconId CategoryDmiChassis::Icon() const
{
    return IDI_SERVER;
}

const char* CategoryDmiChassis::Guid() const
{
    return "81D9E51F-4A86-49FC-A37F-232D6A62EC45";
}

void CategoryDmiChassis::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiChassis::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiCaches
//

const char* CategoryDmiCaches::Name() const
{
    return "Caches";
}

Category::IconId CategoryDmiCaches::Icon() const
{
    return IDI_CHIP;
}

const char* CategoryDmiCaches::Guid() const
{
    return "BA9258E7-0046-4A77-A97B-0407453706A3";
}

void CategoryDmiCaches::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiCaches::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiProcessors
//

const char* CategoryDmiProcessors::Name() const
{
    return "Processors";
}

Category::IconId CategoryDmiProcessors::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryDmiProcessors::Guid() const
{
    return "84D8B0C3-37A4-4825-A523-40B62E0CADC3";
}

void CategoryDmiProcessors::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiProcessors::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiMemoryDevices
//

const char* CategoryDmiMemoryDevices::Name() const
{
    return "Memory Devices";
}

Category::IconId CategoryDmiMemoryDevices::Icon() const
{
    return IDI_MEMORY;
}

const char* CategoryDmiMemoryDevices::Guid() const
{
    return "9C591459-A83F-4F48-883D-927765C072B0";
}

void CategoryDmiMemoryDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiMemoryDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiSystemSlots
//

const char* CategoryDmiSystemSlots::Name() const
{
    return "System Slots";
}

Category::IconId CategoryDmiSystemSlots::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiSystemSlots::Guid() const
{
    return "7A4F71C6-557F-48A5-AC94-E430F69154F1";
}

void CategoryDmiSystemSlots::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiSystemSlots::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiPortConnectors
//

const char* CategoryDmiPortConnectors::Name() const
{
    return "Port Connectors";
}

Category::IconId CategoryDmiPortConnectors::Icon() const
{
    return IDI_PORT;
}

const char* CategoryDmiPortConnectors::Guid() const
{
    return "FF4CE0FE-261F-46EF-852F-42420E68CFD2";
}

void CategoryDmiPortConnectors::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiPortConnectors::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiOnboardDevices
//

const char* CategoryDmiOnboardDevices::Name() const
{
    return "Onboard Devices";
}

Category::IconId CategoryDmiOnboardDevices::Icon() const
{
    return IDI_MOTHERBOARD;
}

const char* CategoryDmiOnboardDevices::Guid() const
{
    return "6C62195C-5E5F-41BA-B6AD-99041594DAC6";
}

void CategoryDmiOnboardDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiOnboardDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiBuildinPointing
//

const char* CategoryDmiBuildinPointing::Name() const
{
    return "Build-in Pointing";
}

Category::IconId CategoryDmiBuildinPointing::Icon() const
{
    return IDI_MOUSE;
}

const char* CategoryDmiBuildinPointing::Guid() const
{
    return "6883684B-3CEC-451B-A2E3-34C16348BA1B";
}

void CategoryDmiBuildinPointing::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiBuildinPointing::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryDmiPortableBattery
//

const char* CategoryDmiPortableBattery::Name() const
{
    return "Portable Battery";
}

Category::IconId CategoryDmiPortableBattery::Icon() const
{
    return IDI_BATTERY;
}

const char* CategoryDmiPortableBattery::Guid() const
{
    return "0CA213B5-12EE-4828-A399-BA65244E65FD";
}

void CategoryDmiPortableBattery::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryDmiPortableBattery::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupDMI
//

const char* CategoryGroupDMI::Name() const
{
    return "DMI";
}

Category::IconId CategoryGroupDMI::Icon() const
{
    return IDI_COMPUTER;
}

//
// CategoryCPU
//

const char* CategoryCPU::Name() const
{
    return "Central Processor";
}

Category::IconId CategoryCPU::Icon() const
{
    return IDI_PROCESSOR;
}

const char* CategoryCPU::Guid() const
{
    return "31D1312E-85A9-419A-91B4-BA81129B3CCC";
}

void CategoryCPU::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryCPU::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryOpticalDrives
//

const char* CategoryOpticalDrives::Name() const
{
    return "Optical Drives";
}

Category::IconId CategoryOpticalDrives::Icon() const
{
    return IDI_DRIVE_DISK;
}

const char* CategoryOpticalDrives::Guid() const
{
    return "68E028FE-3DA6-4BAF-9E18-CDB828372860";
}

void CategoryOpticalDrives::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryOpticalDrives::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryATA
//

const char* CategoryATA::Name() const
{
    return "ATA";
}

Category::IconId CategoryATA::Icon() const
{
    return IDI_DRIVE;
}

const char* CategoryATA::Guid() const
{
    return "79D80586-D264-46E6-8718-09E267730B78";
}

void CategoryATA::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryATA::Serialize()
{
    // TODO
    return std::string();
}

//
// CategorySMART
//

const char* CategorySMART::Name() const
{
    return "S.M.A.R.T.";
}

Category::IconId CategorySMART::Icon() const
{
    return IDI_DRIVE;
}

const char* CategorySMART::Guid() const
{
    return "7B1F2ED7-7A2E-4F5C-A70B-A56AB5B8CE00";
}

void CategorySMART::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategorySMART::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupStorage
//

const char* CategoryGroupStorage::Name() const
{
    return "Storage";
}

Category::IconId CategoryGroupStorage::Icon() const
{
    return IDI_DRIVE;
}

//
// CategoryWindowsVideo
//

const char* CategoryWindowsVideo::Name() const
{
    return "Windows Video";
}

Category::IconId CategoryWindowsVideo::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryWindowsVideo::Guid() const
{
    return "09E9069D-C394-4CD7-8252-E5CF83B7674C";
}

void CategoryWindowsVideo::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryWindowsVideo::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryMonitor
//

const char* CategoryMonitor::Name() const
{
    return "Monitor";
}

Category::IconId CategoryMonitor::Icon() const
{
    return IDI_MONITOR;
}

const char* CategoryMonitor::Guid() const
{
    return "281100E4-88ED-4AE2-BC4A-3A37282BBAB5";
}

void CategoryMonitor::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryMonitor::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryOpenGL
//

const char* CategoryOpenGL::Name() const
{
    return "OpenGL";
}

Category::IconId CategoryOpenGL::Icon() const
{
    return IDI_CLAPPERBOARD;
}

const char* CategoryOpenGL::Guid() const
{
    return "05E4437C-A0CD-41CB-8B50-9A627E13CB97";
}

void CategoryOpenGL::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryOpenGL::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupDisplay
//

const char* CategoryGroupDisplay::Name() const
{
    return "Display";
}

Category::IconId CategoryGroupDisplay::Icon() const
{
    return IDI_MONITOR;
}

//
// CategoryPrinters
//

const char* CategoryPrinters::Name() const
{
    return "Printers";
}

Category::IconId CategoryPrinters::Icon() const
{
    return IDI_PRINTER;
}

const char* CategoryPrinters::Guid() const
{
    return "ACBDCE39-CE38-4A79-9626-8C8BA2E3A26A";
}

void CategoryPrinters::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    system_info::Printers message;

    if (!message.ParseFromString(data))
        return;

    Output::Table table(output, Name());

    {
        Output::TableHeader header(output);
        output->AddHeaderItem("Parameter", 250);
        output->AddHeaderItem("Value", 250);
    }

    for (int index = 0; index < message.item_size(); ++index)
    {
        const system_info::Printers::Item& item = message.item(index);

        Output::Group group(output, item.name(), Icon());

        output->AddParam(IDI_PRINTER, "Default Printer", item.is_default() ? "Yes" : "No");
        output->AddParam(IDI_PRINTER_SHARE, "Shared Printer", item.is_shared() ? "Yes" : "No");
        output->AddParam(IDI_PORT, "Port", item.port_name());
        output->AddParam(IDI_PCI, "Driver", item.driver_name());
        output->AddParam(IDI_PCI, "Device Name", item.device_name());
        output->AddParam(IDI_PRINTER, "Print Processor", item.print_processor());
        output->AddParam(IDI_PRINTER, "Data Type", item.data_type());
        output->AddParam(IDI_PRINTER, "Print Jobs Queued", std::to_string(item.jobs_count()));

        if (item.paper_width())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Paper Width",
                             std::to_string(item.paper_width()),
                             "mm");
        }

        if (item.paper_length())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Paper Length",
                             std::to_string(item.paper_length()),
                             "mm");
        }

        if (item.print_quality())
        {
            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Print Quality",
                             std::to_string(item.print_quality()),
                             "dpi");
        }

        switch (item.orientation())
        {
            case system_info::Printers::Item::ORIENTATION_LANDSCAPE:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Landscape");
                break;

            case system_info::Printers::Item::ORIENTATION_PORTRAIT:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Portrait");
                break;

            default:
                output->AddParam(IDI_DOCUMENT_TEXT, "Orientation", "Unknown");
                break;
        }
    }
}

std::string CategoryPrinters::Serialize()
{
    system_info::Printers message;

    for (PrinterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        system_info::Printers::Item* item = message.add_item();

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
                item->set_orientation(system_info::Printers::Item::ORIENTATION_PORTRAIT);
                break;

            case PrinterEnumerator::Orientation::LANDSCAPE:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_LANDSCAPE);
                break;

            default:
                item->set_orientation(system_info::Printers::Item::ORIENTATION_UNKNOWN);
                break;
        }
    }

    return message.SerializeAsString();
}

//
// CategoryPowerOptions
//

const char* CategoryPowerOptions::Name() const
{
    return "Power Options";
}

Category::IconId CategoryPowerOptions::Icon() const
{
    return IDI_POWER_SUPPLY;
}

const char* CategoryPowerOptions::Guid() const
{
    return "42E04A9E-36F7-42A1-BCDA-F3ED70112DFF";
}

void CategoryPowerOptions::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryPowerOptions::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryAllDevices
//

const char* CategoryAllDevices::Name() const
{
    return "All Devices";
}

Category::IconId CategoryAllDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryAllDevices::Guid() const
{
    return "22C4F1A6-67F2-4445-B807-9D39E1A80636";
}

void CategoryAllDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryAllDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryUnknownDevices
//

const char* CategoryUnknownDevices::Name() const
{
    return "Unknown Devices";
}

Category::IconId CategoryUnknownDevices::Icon() const
{
    return IDI_PCI;
}

const char* CategoryUnknownDevices::Guid() const
{
    return "5BE9FAA9-5F94-4420-8650-B649F35A1DA0";
}

void CategoryUnknownDevices::Parse(std::shared_ptr<OutputProxy> output, const std::string& data)
{
    UNUSED_PARAMETER(output);
    UNUSED_PARAMETER(data);
    // TODO
}

std::string CategoryUnknownDevices::Serialize()
{
    // TODO
    return std::string();
}

//
// CategoryGroupWindowDevices
//

const char* CategoryGroupWindowDevices::Name() const
{
    return "Windows Devices";
}

Category::IconId CategoryGroupWindowDevices::Icon() const
{
    return IDI_PCI;
}

//
// CategoryGroupHardware
//

const char* CategoryGroupHardware::Name() const
{
    return "Hardware";
}

Category::IconId CategoryGroupHardware::Icon() const
{
    return IDI_HARDWARE;
}

} // namespace aspia
