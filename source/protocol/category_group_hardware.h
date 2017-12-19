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
