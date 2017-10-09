//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_dmi.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_dmi.h"
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

CategoryGroupDMI::CategoryGroupDMI()
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

} // namespace aspia
