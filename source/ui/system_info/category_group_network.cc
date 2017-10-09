//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_network.h"
#include "ui/system_info/category_info.h"

namespace aspia {

class CategoryNetworkCards : public CategoryInfo
{
public:
    CategoryNetworkCards()
        : CategoryInfo(system_info::network::kNetworkCards,
                       IDS_SI_CATEGORY_NETWORK_CARDS,
                       IDI_NETWORK_ADAPTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryNetworkCards() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryNetworkCards);
};

class CategoryRasConnections : public CategoryInfo
{
public:
    CategoryRasConnections()
        : CategoryInfo(system_info::network::kRASConnections,
                       IDS_SI_CATEGORY_RAS_CONNECTIONS,
                       IDI_TELEPHONE_FAX)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryRasConnections() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRasConnections);
};

class CategoryOpenConnections : public CategoryInfo
{
public:
    CategoryOpenConnections()
        : CategoryInfo(system_info::network::kOpenConnections,
                       IDS_SI_CATEGORY_OPEN_CONNECTIONS,
                       IDI_SERVERS_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpenConnections() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenConnections);
};

class CategorySharedResources : public CategoryInfo
{
public:
    CategorySharedResources()
        : CategoryInfo(system_info::network::kSharedResources,
                       IDS_SI_CATEGORY_SHARED_RESOURCES,
                       IDI_FOLDER_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategorySharedResources() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySharedResources);
};

class CategoryOpenFiles : public CategoryInfo
{
public:
    CategoryOpenFiles()
        : CategoryInfo(system_info::network::kOpenFiles,
                       IDS_SI_CATEGORY_OPEN_FILES,
                       IDI_FOLDER_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryOpenFiles() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenFiles);
};

class CategoryRoutes : public CategoryInfo
{
public:
    CategoryRoutes()
        : CategoryInfo(system_info::network::kRoutes,
                       IDS_SI_CATEGORY_ROUTES,
                       IDI_ROUTE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
        column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
    }

    ~CategoryRoutes() = default;

    void Parse(const std::string& data, Output* output) final
    {
        // TODO
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRoutes);
};

CategoryGroupNetwork::CategoryGroupNetwork()
    : CategoryGroup(IDS_SI_CATEGORY_NETWORK, IDI_NETWORK)
{
    CategoryList* child_list = mutable_child_list();
    child_list->emplace_back(std::make_unique<CategoryNetworkCards>());
    child_list->emplace_back(std::make_unique<CategoryRasConnections>());
    child_list->emplace_back(std::make_unique<CategoryOpenConnections>());
    child_list->emplace_back(std::make_unique<CategorySharedResources>());
    child_list->emplace_back(std::make_unique<CategoryOpenFiles>());
    child_list->emplace_back(std::make_unique<CategoryRoutes>());
}

} // namespace aspia
