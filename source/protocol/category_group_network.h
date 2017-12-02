//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/category_group_network.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H
#define _ASPIA_PROTOCOL__CATEGORY_GROUP_NETWORK_H

#include "base/macros.h"
#include "proto/system_info_session_message.pb.h"
#include "protocol/category.h"

namespace aspia {

class CategoryNetworkCards : public CategoryInfo
{
public:
    CategoryNetworkCards() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryNetworkCards);
};

class CategoryRasConnections : public CategoryInfo
{
public:
    CategoryRasConnections() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRasConnections);
};

class CategoryOpenConnections : public CategoryInfo
{
public:
    CategoryOpenConnections() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenConnections);
};

class CategorySharedResources : public CategoryInfo
{
public:
    CategorySharedResources() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
    std::string Serialize() final;

private:
    static const char* TypeToString(proto::SharedResources::Item::Type type);

    DISALLOW_COPY_AND_ASSIGN(CategorySharedResources);
};

class CategoryOpenFiles : public CategoryInfo
{
public:
    CategoryOpenFiles() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenFiles);
};

class CategoryRoutes : public CategoryInfo
{
public:
    CategoryRoutes() = default;

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Output* output, const std::string& data) final;
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
