//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_hardware.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_hardware.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryDmi : public CategoryInfo
{
public:
    CategoryDmi(const char* guid, UINT name_id, UINT icon_id);
    virtual ~CategoryDmi() = default;
};

CategoryDmi::CategoryDmi(const char* guid, UINT name_id, UINT icon_id)
    : CategoryInfo(guid, name_id, icon_id)
{
    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

class CategoryDmiBios : public CategoryDmi
{
public:
    CategoryDmiBios()
        : CategoryDmi(system_info::hardware::dmi::kBIOS,
                      IDS_SI_CATEGORY_DMI_BIOS,
                      IDI_BIOS)
    {
        // Nothing
    }

    ~CategoryDmiBios() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBios);
};

class CategoryDmiSystem : public CategoryDmi
{
public:
    CategoryDmiSystem()
        : CategoryDmi(system_info::hardware::dmi::kSystem,
                      IDS_SI_CATEGORY_DMI_SYSTEM,
                      IDI_COMPUTER)
    {
        // Nothing
    }

    ~CategoryDmiSystem() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryDmi(system_info::hardware::dmi::kMotherboard,
                      IDS_SI_CATEGORY_DMI_MOTHERBOARD,
                      IDI_MOTHERBOARD)
    {
        // Nothing
    }

    ~CategoryDmiMotherboard() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiMotherboard);
};

class CategoryDmiChassis : public CategoryDmi
{
public:
    CategoryDmiChassis()
        : CategoryDmi(system_info::hardware::dmi::kChassis,
                      IDS_SI_CATEGORY_DMI_CHASSIS,
                      IDI_SERVER)
    {
        // Nothing
    }

    ~CategoryDmiChassis() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiChassis);
};

class CategoryDmiCaches : public CategoryDmi
{
public:
    CategoryDmiCaches()
        : CategoryDmi(system_info::hardware::dmi::kCaches,
                      IDS_SI_CATEGORY_DMI_CACHES,
                      IDI_CHIP)
    {
        // Nothing
    }

    ~CategoryDmiCaches() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryDmi(system_info::hardware::dmi::kProcessors,
                      IDS_SI_CATEGORY_DMI_PROCESSORS,
                      IDI_PROCESSOR)
    {
        // Nothing
    }

    ~CategoryDmiProcessors() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryDmi(system_info::hardware::dmi::kMemoryDevices,
                      IDS_SI_CATEGORY_DMI_MEMORY_DEVICES,
                      IDI_MEMORY)
    {
        // Nothing
    }

    ~CategoryDmiMemoryDevices() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryDmi(system_info::hardware::dmi::kSystemSlots,
                      IDS_SI_CATEGORY_DMI_SYSTEM_SLOTS,
                      IDI_PORT)
    {
        // Nothing
    }

    ~CategoryDmiSystemSlots() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryDmi(system_info::hardware::dmi::kPortConnectors,
                      IDS_SI_CATEGORY_DMI_PORT_CONNECTORS,
                      IDI_PORT)
    {
        // Nothing
    }

    ~CategoryDmiPortConnectors() = default;

    void Parse(const std::string& data, Output* output) final
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
                      IDS_SI_CATEGORY_DMI_ONBOARD_DEVICES,
                      IDI_MOTHERBOARD)
    {
        // Nothing
    }

    ~CategoryDmiOnboardDevices() = default;

    void Parse(const std::string& data, Output* output) final
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
                      IDS_SI_CATEGORY_DMI_BUILDIN_POINTING,
                      IDI_MOUSE)
    {
        // Nothing
    }

    ~CategoryDmiBuildinPointing() = default;

    void Parse(const std::string& data, Output* output) final
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
                      IDS_SI_CATEGORY_DMI_PORTABLE_BATTERY,
                      IDI_BATTERY)
    {
        // Nothing
    }

    ~CategoryDmiPortableBattery() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryGroup(IDS_SI_CATEGORY_DMI, IDI_COMPUTER)
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
        : CategoryInfo(system_info::hardware::kCPU,
                       IDS_SI_CATEGORY_CPU,
                       IDI_PROCESSOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryCPU() = default;

    void Parse(const std::string& data, Output* output) final
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
                       IDS_SI_CATEGORY_STORAGE_OPTICAL_DRIVES,
                       IDI_DRIVE_DISK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpticalDrives() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpticalDrives);
};

class CategoryATA : public CategoryInfo
{
public:
    CategoryATA()
        : CategoryInfo(system_info::hardware::storage::kATA,
                       IDS_SI_CATEGORY_STORAGE_ATA,
                       IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryATA() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryInfo(system_info::hardware::storage::kSMART,
                       IDS_SI_CATEGORY_STORAGE_SMART,
                       IDI_DRIVE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategorySMART() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryGroup(IDS_SI_CATEGORY_STORAGE, IDI_DRIVE)
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
                       IDS_SI_CATEGORY_DISPLAY_WINDOWS_VIDEO,
                       IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryWindowsVideo() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryInfo(system_info::hardware::display::kMonitor,
                       IDS_SI_CATEGORY_DISPLAY_MONITOR,
                       IDI_MONITOR)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryMonitor() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryInfo(system_info::hardware::display::kOpenGL,
                       IDS_SI_CATEGORY_DISPLAY_OPENGL,
                       IDI_CLAPPERBOARD)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpenGL() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenGL);
};

class CategoryGroupDisplay : public CategoryGroup
{
public:
    CategoryGroupDisplay()
        : CategoryGroup(IDS_SI_CATEGORY_DISPLAY, IDI_MONITOR)
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
    CategoryPrinters()
        : CategoryInfo(system_info::hardware::kPrinters,
                       IDS_SI_CATEGORY_PRINTERS,
                       IDI_PRINTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryPrinters() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrinters);
};

class CategoryPowerOptions : public CategoryInfo
{
public:
    CategoryPowerOptions()
        : CategoryInfo(system_info::hardware::kPowerOptions,
                       IDS_SI_CATEGORY_POWER_OPTIONS,
                       IDI_POWER_SUPPLY)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryPowerOptions() = default;

    void Parse(const std::string& data, Output* output) final
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
        : CategoryInfo(system_info::hardware::windows_devices::kAll,
                       IDS_SI_CATEGORY_WINDOWS_DEVICES_ALL,
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryAllDevices() = default;

    void Parse(const std::string& data, Output* output) final
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
                       IDS_SI_CATEGORY_WINDOWS_DEVICES_UNK,
                       IDI_PCI)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryUnknownDevices() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUnknownDevices);
};

class CategoryGroupWindowDevices : public CategoryGroup
{
public:
    CategoryGroupWindowDevices()
        : CategoryGroup(IDS_SI_CATEGORY_WINDOWS_DEVICES, IDI_PCI)
    {
        CategoryList* child_list = mutable_child_list();
        child_list->emplace_back(std::make_unique<CategoryAllDevices>());
        child_list->emplace_back(std::make_unique<CategoryUnknownDevices>());
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupWindowDevices);
};

CategoryGroupHardware::CategoryGroupHardware()
    : CategoryGroup(IDS_SI_CATEGORY_HARDWARE, IDI_HARDWARE)
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
