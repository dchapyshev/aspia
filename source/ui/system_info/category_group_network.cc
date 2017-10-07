//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"
#include "ui/system_info/category_group_network.h"

namespace aspia {

//
// Network Cards
//

class CategoryNetworkCards : public Category
{
public:
    CategoryNetworkCards();
    ~CategoryNetworkCards() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryNetworkCards);
};

CategoryNetworkCards::CategoryNetworkCards()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_NETWORK_CARDS, IDI_NETWORK_ADAPTER)
{
    set_guid(system_info::network::kNetworkCards);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// RAS Connections
//

class CategoryRasConnections : public Category
{
public:
    CategoryRasConnections();
    ~CategoryRasConnections() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRasConnections);
};

CategoryRasConnections::CategoryRasConnections()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_RAS_CONNECTIONS, IDI_TELEPHONE_FAX)
{
    set_guid(system_info::network::kRASConnections);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Open Connections
//

class CategoryOpenConnections : public Category
{
public:
    CategoryOpenConnections();
    ~CategoryOpenConnections() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenConnections);
};

CategoryOpenConnections::CategoryOpenConnections()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_OPEN_CONNECTIONS, IDI_SERVERS_NETWORK)
{
    set_guid(system_info::network::kOpenConnections);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Shared Resources
//

class CategorySharedResources : public Category
{
public:
    CategorySharedResources();
    ~CategorySharedResources() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategorySharedResources);
};

CategorySharedResources::CategorySharedResources()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_SHARED_RESOURCES, IDI_FOLDER_NETWORK)
{
    set_guid(system_info::network::kSharedResources);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Open Files
//

class CategoryOpenFiles : public Category
{
public:
    CategoryOpenFiles();
    ~CategoryOpenFiles() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenFiles);
};

CategoryOpenFiles::CategoryOpenFiles()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_OPEN_FILES, IDI_FOLDER_NETWORK)
{
    set_guid(system_info::network::kOpenFiles);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Routes
//

class CategoryRoutes : public Category
{
public:
    CategoryRoutes();
    ~CategoryRoutes() = default;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRoutes);
};

CategoryRoutes::CategoryRoutes()
    : Category(Type::REGULAR, IDS_SI_CATEGORY_ROUTES, IDI_ROUTE)
{
    set_guid(system_info::network::kRoutes);

    ColumnList* column_list = mutable_column_list();
    column_list->emplace_back(IDS_SI_COLUMN_PARAMETER, 200);
    column_list->emplace_back(IDS_SI_COLUMN_VALUE, 200);
}

//
// Network Group
//

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
