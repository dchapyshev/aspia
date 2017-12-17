//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_hardware.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_HARDWARE_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_HARDWARE_H

#include "base/macros.h"
#include "proto/system_info_session_message.pb.h"
#include "protocol/category.h"

namespace aspia {

class CategoryDmiBios : public CategoryInfo
{
public:
    CategoryDmiBios() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBios);
};

class CategoryDmiSystem : public CategoryInfo
{
public:
    CategoryDmiSystem() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* WakeupTypeToString(proto::DmiSystem::WakeupType value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiSystem);
};

class CategoryDmiBaseboard : public CategoryInfo
{
public:
    CategoryDmiBaseboard() : CategoryInfo((Type::INFO_PARAM_VALUE)) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* BoardTypeToString(proto::DmiBaseboard::BoardType type);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiBaseboard);
};

class CategoryDmiChassis : public CategoryInfo
{
public:
    CategoryDmiChassis() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiChassis:: Type type);
    static const char* StatusToString(proto::DmiChassis::Status status);
    static const char* SecurityStatusToString(proto::DmiChassis::SecurityStatus status);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiChassis);
};

class CategoryDmiCaches : public CategoryInfo
{
public:
    CategoryDmiCaches() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* LocationToString(proto::DmiCaches::Location value);
    static const char* StatusToString(proto::DmiCaches::Status value);
    static const char* ModeToString(proto::DmiCaches::Mode value);
    static const char* SRAMTypeToString(proto::DmiCaches::SRAMType value);
    static const char* ErrorCorrectionTypeToString(proto::DmiCaches::ErrorCorrectionType value);
    static const char* TypeToString(proto::DmiCaches::Type value);
    static const char* AssociativityToString(proto::DmiCaches::Associativity value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiCaches);
};

class CategoryDmiProcessors : public CategoryInfo
{
public:
    CategoryDmiProcessors() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* FamilyToString(proto::DmiProcessors::Family value);
    static const char* TypeToString(proto::DmiProcessors::Type value);
    static const char* StatusToString(proto::DmiProcessors::Status value);
    static const char* UpgradeToString(proto::DmiProcessors::Upgrade value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiProcessors);
};

class CategoryDmiMemoryDevices : public CategoryInfo
{
public:
    CategoryDmiMemoryDevices() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiMemoryDevices::Type value);
    static const char* FormFactorToString(proto::DmiMemoryDevices::FormFactor value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiMemoryDevices);
};

class CategoryDmiSystemSlots : public CategoryInfo
{
public:
    CategoryDmiSystemSlots() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiSystemSlots::Type value);
    static const char* UsageToString(proto::DmiSystemSlots::Usage value);
    static const char* BusWidthToString(proto::DmiSystemSlots::BusWidth value);
    static const char* LengthToString(proto::DmiSystemSlots::Length value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiSystemSlots);
};

class CategoryDmiPortConnectors : public CategoryInfo
{
public:
    CategoryDmiPortConnectors() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiPortConnectors::Type value);
    static const char* ConnectorTypeToString(proto::DmiPortConnectors::ConnectorType value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPortConnectors);
};

class CategoryDmiOnboardDevices : public CategoryInfo
{
public:
    CategoryDmiOnboardDevices() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiOnBoardDevices::Type value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiOnboardDevices);
};

class CategoryDmiPointingDevices : public CategoryInfo
{
public:
    CategoryDmiPointingDevices() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::DmiPointingDevices::Type value);
    static const char* InterfaceToString(proto::DmiPointingDevices::Interface value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPointingDevices);
};

class CategoryDmiPortableBattery : public CategoryInfo
{
public:
    CategoryDmiPortableBattery() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* ChemistryToString(proto::DmiPortableBattery::Chemistry value);

    DISALLOW_COPY_AND_ASSIGN(CategoryDmiPortableBattery);
};

class CategoryGroupDMI : public CategoryGroup
{
public:
    CategoryGroupDMI() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDMI);
};

class CategoryCPU : public CategoryInfo
{
public:
    CategoryCPU() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryCPU);
};

class CategoryOpticalDrives : public CategoryInfo
{
public:
    CategoryOpticalDrives() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpticalDrives);
};

class CategoryATA : public CategoryInfo
{
public:
    CategoryATA() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* BusTypeToString(proto::AtaDrives::BusType value);
    static const char* TransferModeToString(proto::AtaDrives::TransferMode value);

    DISALLOW_COPY_AND_ASSIGN(CategoryATA);
};

class CategorySMART : public CategoryInfo
{
public:
    CategorySMART() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    enum class DriveType
    {
        GENERIC            = 0,
        INTEL_SSD          = 1,
        SAMSUNG_SSD        = 2,
        SAND_FORCE_SSD     = 3,
        KINGSTON_UV400_SSD = 4,
        MICRON_MU02_SSD    = 5,
        MICRON_SSD         = 6
    };

    static bool IsIntelSSD(const proto::SMART::Drive& drive);
    static bool IsMicronMU02SSD(const proto::SMART::Drive& drive);
    static bool IsMicronSSD(const proto::SMART::Drive& drive);
    static bool IsSamsungSSD(const proto::SMART::Drive& drive);
    static bool IsKingstonUV400(const proto::SMART::Drive& drive);
    static bool IsSandForceSSD(const proto::SMART::Drive& drive);

    static DriveType GetDriveType(const proto::SMART::Drive& drive);

    static const char* AttributeToString(DriveType type, uint32_t value);
    static const char* GenericAttributeToString(uint32_t value);
    static const char* IntelAttributeToString(uint32_t value);
    static const char* MicronMU02AttributeToString(uint32_t value);
    static const char* MicronAttributeToString(uint32_t value);
    static const char* SamsungAttributeToString(uint32_t value);
    static const char* KingstonUV400AttributeToString(uint32_t value);
    static const char* SandForceAttributeToString(uint32_t value);

    DISALLOW_COPY_AND_ASSIGN(CategorySMART);
};

class CategoryGroupStorage : public CategoryGroup
{
public:
    CategoryGroupStorage() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupStorage);
};

class CategoryVideoAdapters : public CategoryInfo
{
public:
    CategoryVideoAdapters() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryVideoAdapters);
};

class CategoryMonitor : public CategoryInfo
{
public:
    CategoryMonitor() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* InputSignalTypeToString(proto::Monitors::InputSignalType value);

    DISALLOW_COPY_AND_ASSIGN(CategoryMonitor);
};

class CategoryGroupDisplay : public CategoryGroup
{
public:
    CategoryGroupDisplay() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupDisplay);
};

class CategoryPrinters : public CategoryInfo
{
public:
    CategoryPrinters() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrinters);
};

class CategoryPowerOptions : public CategoryInfo
{
public:
    CategoryPowerOptions() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPowerOptions);
};

class CategoryWindowsDevices : public CategoryInfo
{
public:
    CategoryWindowsDevices() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryWindowsDevices);
};

class CategoryGroupHardware : public CategoryGroup
{
public:
    CategoryGroupHardware() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupHardware);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_HARDWARE_H
