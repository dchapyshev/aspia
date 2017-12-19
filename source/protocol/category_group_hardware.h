//
// PROJECT:         Aspia
// FILE:            protocol/category_group_hardware.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_HARDWARE_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_HARDWARE_H

#include "base/macros.h"
#include "proto/system_info_session_message.pb.h"
#include "system_info/category.h"

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

class CategoryGroupStorage : public CategoryGroup
{
public:
    CategoryGroupStorage() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupStorage);
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
