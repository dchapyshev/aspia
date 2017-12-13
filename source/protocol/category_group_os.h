//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_os.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_OS_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_OS_H

#include "base/macros.h"
#include "protocol/category.h"

namespace aspia {

class CategoryTaskScheduler : public CategoryInfo
{
public:
    CategoryTaskScheduler() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryTaskScheduler);
};

class CategoryEnvironmentVariables : public CategoryInfo
{
public:
    CategoryEnvironmentVariables() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEnvironmentVariables);
};

class CategoryEventLogsApplications : public CategoryInfo
{
public:
    CategoryEventLogsApplications() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEventLogsApplications);
};

class CategoryEventLogsSecurity : public CategoryInfo
{
public:
    CategoryEventLogsSecurity() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEventLogsSecurity);
};

class CategoryEventLogsSystem : public CategoryInfo
{
public:
    CategoryEventLogsSystem() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEventLogsSystem);
};

class CategoryGroupEventLog : public CategoryGroup
{
public:
    CategoryGroupEventLog() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupEventLog);
};

class CategoryUsers : public CategoryInfo
{
public:
    CategoryUsers() : CategoryInfo(Type::INFO_PARAM_VALUE) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUsers);
};

class CategoryUserGroups : public CategoryInfo
{
public:
    CategoryUserGroups() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryUserGroups);
};

class CategoryActiveSessions : public CategoryInfo
{
public:
    CategoryActiveSessions() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryActiveSessions);
};

class CategoryGroupUsers : public CategoryGroup
{
public:
    CategoryGroupUsers() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupUsers);
};

class CategoryGroupOS : public CategoryGroup
{
public:
    CategoryGroupOS() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupOS);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_OS_H
