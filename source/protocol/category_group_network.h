//
// PROJECT:         Aspia
// FILE:            protocol/category_group_network.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H

#include "base/macros.h"
#include "proto/system_info_session_message.pb.h"
#include "system_info/category.h"

namespace aspia {

class CategoryOpenFiles : public CategoryInfo
{
public:
    CategoryOpenFiles() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenFiles);
};

class CategoryRoutes : public CategoryInfo
{
public:
    CategoryRoutes() : CategoryInfo(Type::INFO_LIST) { /* Nothing */ }

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRoutes);
};

class CategoryGroupNetwork : public CategoryGroup
{
public:
    CategoryGroupNetwork() = default;

    const char* Name() const final;
    IconId Icon() const final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupNetwork);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H
