//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/category_group_hardware.h"
#include "ui/system_info/category_info.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

class CategoryDmi : public CategoryInfo
{
public:
    CategoryDmi(const char* guid, const std::string& name, int icon_id)
        : CategoryInfo(guid, name, icon_id)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }
};

class CategoryDmiBios : public CategoryDmi
{
public:
    CategoryDmiBios() : CategoryDmi(system_info::hardware::dmi::kBIOS, "BIOS", IDI_BIOS)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBios);
};

class CategoryDmiSystem : public CategoryDmi
{
public:
    CategoryDmiSystem() : CategoryDmi(system_info::hardware::dmi::kSystem, "System", IDI_COMPUTER)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiSystem);
};

class CategoryDmiMotherboard : public CategoryDmi
{
public:
    CategoryDmiMotherboard()
        : CategoryDmi(system_info::hardware::dmi::kMotherboard, "Motherboard", IDI_MOTHERBOARD)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiMotherboard);
};

class CategoryDmiChassis : public CategoryDmi
{
public:
    CategoryDmiChassis() : CategoryDmi(system_info::hardware::dmi::kChassis, "Chassis", IDI_SERVER)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiChassis);
};

class CategoryDmiCaches : public CategoryDmi
{
public:
    CategoryDmiCaches() : CategoryDmi(system_info::hardware::dmi::kCaches, "Caches", IDI_CHIP)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiCaches);
};

class CategoryDmiProcessors : public CategoryDmi
{
public:
    CategoryDmiProcessors()
        : CategoryDmi(system_info::hardware::dmi::kProcessors, "Processors", IDI_PROCESSOR)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiProcessors);
};

class CategoryDmiMemoryDevices : public CategoryDmi
{
public:
    CategoryDmiMemoryDevices()
        : CategoryDmi(system_info::hardware::dmi::kMemoryDevices, "Memory Devices", IDI_MEMORY)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiMemoryDevices);
};

class CategoryDmiSystemSlots : public CategoryDmi
{
public:
    CategoryDmiSystemSlots()
        : CategoryDmi(system_info::hardware::dmi::kSystemSlots, "System Slots", IDI_PORT)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiSystemSlots);
};

class CategoryDmiPortConnectors : public CategoryDmi
{
public:
    CategoryDmiPortConnectors()
        : CategoryDmi(system_info::hardware::dmi::kPortConnectors, "Port Connectors", IDI_PORT)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPortConnectors);
};

class CategoryDmiOnboardDevices : public CategoryDmi
{
public:
    CategoryDmiOnboardDevices()
        : CategoryDmi(system_info::hardware::dmi::kOnboardDevices,
                      "Onboard Devices",
                      IDI_MOTHERBOARD)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiOnboardDevices);
};

class CategoryDmiBuildinPointing : public CategoryDmi
{
public:
    CategoryDmiBuildinPointing()
        : CategoryDmi(system_info::hardware::dmi::kBuildinPointing,
                      "Build-in Pointing",
                      IDI_MOUSE)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBuildinPointing);
};

class CategoryDmiPortableBattery : public CategoryDmi
{
public:
    CategoryDmiPortableBattery()
        : CategoryDmi(system_info::hardware::dmi::kPortableBattery,
                      "Portable Battery",
                      IDI_BATTERY)
    {
        // Nothing
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPortableBattery);
};

class CategoryGroupDMI : public CategoryGroup
{
public:
    CategoryGroupDMI()
        : CategoryGroup("DMI", IDI_COMPUTER)
    {
        CategoryList* child_list = mutable_child_list();

        child_list->emplace_back(std::make_unique<CategoryDmiBios>());
        child_list->emplace_back(std::make_unique<CategoryDmiSystem>());
        child_list->emplace_back(std::make_unique<CategoryDmiMotherboard>());
        child_list->emplace_back(std::make_unique<CategoryDmiChassis>());
        child_list->emplace_back(std::make_unique<CategoryDmiCaches>());
        child_list->emplace_back(std::make_unique<CategoryDmiProcessors>());
        child_list->emplace_back(std::make_unique<CategoryDmiMemoryDevices>());
        child_list->emplace_back(std::make_unique<CategoryDmiSystemSlots>());
        child_list->emplace_back(std::make_unique<CategoryDmiPortConnectors>());
        child_list->emplace_back(std::make_unique<CategoryDmiOnboardDevices>());
        child_list->emplace_back(std::make_unique<CategoryDmiBuildinPointing>());
        child_list->emplace_back(std::make_unique<CategoryDmiPortableBattery>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDMI);
};

class CategoryCPU : public CategoryInfo
{
public:
    CategoryCPU()
        : CategoryInfo(system_info::hardware::kCPU, "Central Processor", IDI_PROCESSOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryCPU);
};

class CategoryOpticalDrives : public CategoryInfo
{
public:
    CategoryOpticalDrives()
        : CategoryInfo(system_info::hardware::storage::kOpticalDrives,
                       "Optical Drives",
                       IDI_DRIVE_DISK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    ~CategoryOpticalDrives() = default;

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpticalDrives);
};

class CategoryATA : public CategoryInfo
{
public:
    CategoryATA() : CategoryInfo(system_info::hardware::storage::kATA, "ATA", IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryATA);
};

class CategorySMART : public CategoryInfo
{
public:
    CategorySMART()
        : CategoryInfo(system_info::hardware::storage::kSMART, "S.M.A.R.T.", IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySMART);
};

class CategoryGroupStorage : public CategoryGroup
{
public:
    CategoryGroupStorage()
        : CategoryGroup("Storage", IDI_DRIVE)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryOpticalDrives>());
        child_list->emplace_back(std::make_unique<CategoryATA>());
        child_list->emplace_back(std::make_unique<CategorySMART>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupStorage);
};

class CategoryWindowsVideo : public CategoryInfo
{
public:
    CategoryWindowsVideo()
        : CategoryInfo(system_info::hardware::display::kWindowsVideo,
                       "Windows Video",
                       IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryWindowsVideo);
};

class CategoryMonitor : public CategoryInfo
{
public:
    CategoryMonitor()
        : CategoryInfo(system_info::hardware::display::kMonitor, "Monitor", IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryMonitor);
};

class CategoryOpenGL : public CategoryInfo
{
public:
    CategoryOpenGL()
        : CategoryInfo(system_info::hardware::display::kOpenGL, "OpenGL", IDI_CLAPPERBOARD)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenGL);
};

class CategoryGroupDisplay : public CategoryGroup
{
public:
    CategoryGroupDisplay() : CategoryGroup("Display", IDI_MONITOR)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryWindowsVideo>());
        child_list->emplace_back(std::make_unique<CategoryMonitor>());
        child_list->emplace_back(std::make_unique<CategoryOpenGL>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDisplay);
};

class CategoryPrinters : public CategoryInfo
{
public:
    CategoryPrinters() : CategoryInfo(system_info::hardware::kPrinters, "Printers", IDI_PRINTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 250);
        column_list->emplace_back("Value", 250);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Printers message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

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

            output->AddParam(IDI_DOCUMENT_TEXT,
                             "Orientation",
                             OrientationToString(item.orientation()));
        }
    }

private:
    static const char* OrientationToString(system_info::Printers::Item::Orientation orientation)
    {
        switch (orientation)
        {
            case system_info::Printers::Item::ORIENTATION_LANDSCAPE:
                return "Landscape";

            case system_info::Printers::Item::ORIENTATION_PORTRAIT:
                return "Portrait";

            default:
                return "Unknown";
        }
    }

    DISALLOW_COPY_AND_ASSIGN(CategoryPrinters);
};

class CategoryPowerOptions : public CategoryInfo
{
public:
    CategoryPowerOptions()
        : CategoryInfo(system_info::hardware::kPowerOptions, "Power Options", IDI_POWER_SUPPLY)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPowerOptions);
};

class CategoryAllDevices : public CategoryInfo
{
public:
    CategoryAllDevices()
        : CategoryInfo(system_info::hardware::windows_devices::kAll, "All Devices", IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryAllDevices);
};

class CategoryUnknownDevices : public CategoryInfo
{
public:
    CategoryUnknownDevices()
        : CategoryInfo(system_info::hardware::windows_devices::kUnknown,
                       "Unknown Devices",
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUnknownDevices);
};

class CategoryGroupWindowDevices : public CategoryGroup
{
public:
    CategoryGroupWindowDevices() : CategoryGroup("Windows Devices", IDI_PCI)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryAllDevices>());
        child_list->emplace_back(std::make_unique<CategoryUnknownDevices>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupWindowDevices);
};

CategoryGroupHardware::CategoryGroupHardware() : CategoryGroup("Hardware", IDI_HARDWARE)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryGroupDMI>());
    child_list->emplace_back(std::make_unique<CategoryCPU>());
    child_list->emplace_back(std::make_unique<CategoryGroupStorage>());
    child_list->emplace_back(std::make_unique<CategoryGroupDisplay>());
    child_list->emplace_back(std::make_unique<CategoryPowerOptions>());
    child_list->emplace_back(std::make_unique<CategoryPrinters>());
    child_list->emplace_back(std::make_unique<CategoryGroupWindowDevices>());
}

} // namespace aspia
