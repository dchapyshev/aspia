//
// PROJECT:         Aspia
// FILE:            protocol/category_group_software.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H

#include "base/macros.h"
#include "proto/system_info_session_message.pb.h"
#include "protocol/category.h"

namespace aspia {

class CategoryPrograms : public CategoryInfo
{
public:
    CategoryPrograms() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryPrograms);
};

class CategoryUpdates : public CategoryInfo
{
public:
    CategoryUpdates() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUpdates);
};

class CategoryServices : public CategoryInfo
{
public:
    CategoryServices() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

    static const char* StatusToString(proto::Services::Item::Status status);
    static const char* StartupTypeToString(proto::Services::Item::StartupType startup_type);

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryServices);
};

class CategoryDrivers : public CategoryInfo
{
public:
    CategoryDrivers() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryDrivers);
};

class CategoryProcesses : public CategoryInfo
{
public:
    CategoryProcesses() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryProcesses);
};

class CategoryLicenses : public CategoryInfo
{
public:
    CategoryLicenses() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryLicenses);
};

class CategoryGroupSoftware : public CategoryGroup
{
public:
    CategoryGroupSoftware() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupSoftware);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_SOFTWARE_H
