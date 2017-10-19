//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_network.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_adapter_enumerator.h"
#include "network/open_connection_enumerator.h"
#include "network/share_enumerator.h"
#include "network/route_enumerator.h"
#include "protocol/system_info_constants.h"
#include "proto/system_info_session_message.pb.h"
#include "ui/system_info/category_group_network.h"
#include "ui/system_info/category_info.h"
#include "ui/system_info/output_proxy.h"
#include "ui/resource.h"

namespace aspia {

class CategoryNetworkCards : public CategoryInfo
{
public:
    CategoryNetworkCards()
        : CategoryInfo(system_info::network::kNetworkCards,
                       "Network Cards",
                       IDI_NETWORK_ADAPTER)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 250);
        column_list->emplace_back("Value", 250);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::NetworkCards message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::NetworkCards::Item& item = message.item(index);

            Output::Group group(output, item.adapter_name(), Icon());

            output->AddParam(IDI_NETWORK_ADAPTER, "Connection Name", item.connection_name());
            output->AddParam(IDI_NETWORK_ADAPTER, "Interface Type", item.interface_type());
            output->AddParam(IDI_NETWORK_ADAPTER, "MTU", std::to_string(item.mtu()), "byte");

            output->AddParam(IDI_NETWORK_ADAPTER,
                             "Connection Speed",
                             std::to_string(item.speed() / (1000 * 1000)),
                             "Mbps");

            output->AddParam(IDI_NETWORK_ADAPTER, "MAC Address", item.mac_address());

            if (item.ip_address_size())
            {
                Output::Group addr_group(output, "IP Addresses", IDI_NETWORK_IP);

                for (int addr_index = 0; addr_index < item.ip_address_size(); ++addr_index)
                {
                    const system_info::NetworkCards::Item::IpAddress& address =
                        item.ip_address(addr_index);

                    output->AddParam(IDI_NETWORK_IP,
                                     StringPrintf("Address #%d", addr_index + 1),
                                     address.address());

                    output->AddParam(IDI_NETWORK_IP,
                                     StringPrintf("Mask #%d", addr_index + 1),
                                     address.mask());
                }
            }

            if (item.gateway_address_size())
            {
                Output::Group addr_group(output, "Gateway", IDI_NETWORK_IP);

                for (int addr_index = 0; addr_index < item.gateway_address_size(); ++addr_index)
                {
                    output->AddParam(IDI_NETWORK_IP,
                                     StringPrintf("Address #%d", addr_index + 1),
                                     item.gateway_address(addr_index));
                }
            }

            output->AddParam(IDI_NETWORK_ADAPTER,
                             "DHCP Enabled",
                             item.is_dhcp_enabled() ? "Yes" : "No");

            if (item.is_dhcp_enabled() && item.dhcp_server_address_size())
            {
                Output::Group addr_group(output, "DHCP Server", IDI_NETWORK_IP);

                for (int addr_index = 0; addr_index < item.dhcp_server_address_size(); ++addr_index)
                {
                    output->AddParam(IDI_NETWORK_IP,
                                     StringPrintf("Address #%d", addr_index + 1),
                                     item.dhcp_server_address(addr_index));
                }
            }

            if (item.dns_server_address_size())
            {
                Output::Group addr_group(output, "DNS Server", IDI_NETWORK_IP);

                for (int addr_index = 0; addr_index < item.dns_server_address_size(); ++addr_index)
                {
                    output->AddParam(IDI_NETWORK_IP,
                                     StringPrintf("Address #%d", addr_index + 1),
                                     item.dns_server_address(addr_index));
                }
            }

            output->AddParam(IDI_NETWORK_ADAPTER,
                             "WINS Enabled",
                             item.is_wins_enabled() ? "Yes" : "No");

            if (item.is_wins_enabled())
            {
                output->AddParam(IDI_NETWORK_IP,
                                 "Primary WINS Server",
                                 item.primary_wins_server());

                output->AddParam(IDI_NETWORK_IP,
                                 "Secondary WINS Server",
                                 item.secondary_wins_server());
            }
        }
    }

    std::string Serialize() final
    {
        system_info::NetworkCards message;

        for (NetworkAdapterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::NetworkCards::Item* item = message.add_item();

            item->set_adapter_name(enumerator.GetAdapterName());
            item->set_connection_name(enumerator.GetConnectionName());
            item->set_interface_type(enumerator.GetInterfaceType());
            item->set_mtu(enumerator.GetMtu());
            item->set_speed(enumerator.GetSpeed());
            item->set_mac_address(enumerator.GetMacAddress());
            item->set_is_wins_enabled(enumerator.IsWinsEnabled());
            item->set_primary_wins_server(enumerator.GetPrimaryWinsServer());
            item->set_secondary_wins_server(enumerator.GetSecondaryWinsServer());
            item->set_is_dhcp_enabled(enumerator.IsDhcpEnabled());

            for (NetworkAdapterEnumerator::IpAddressEnumerator ip_address_enumerator(enumerator);
                 !ip_address_enumerator.IsAtEnd();
                 ip_address_enumerator.Advance())
            {
                system_info::NetworkCards::Item::IpAddress* address = item->add_ip_address();

                address->set_address(ip_address_enumerator.GetIpAddress());
                address->set_mask(ip_address_enumerator.GetIpMask());
            }

            for (NetworkAdapterEnumerator::GatewayEnumerator gateway_enumerator(enumerator);
                 !gateway_enumerator.IsAtEnd();
                 gateway_enumerator.Advance())
            {
                item->add_gateway_address(gateway_enumerator.GetAddress());
            }

            for (NetworkAdapterEnumerator::DhcpEnumerator dhcp_enumerator(enumerator);
                 !dhcp_enumerator.IsAtEnd();
                 dhcp_enumerator.Advance())
            {
                item->add_dhcp_server_address(dhcp_enumerator.GetAddress());
            }

            for (NetworkAdapterEnumerator::DnsEnumerator dns_enumerator(enumerator);
                 !dns_enumerator.IsAtEnd();
                 dns_enumerator.Advance())
            {
                item->add_dns_server_address(dns_enumerator.GetAddress());
            }
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryNetworkCards);
};

class CategoryRasConnections : public CategoryInfo
{
public:
    CategoryRasConnections()
        : CategoryInfo(system_info::network::kRASConnections,
                       "RAS Connections",
                       IDI_TELEPHONE_FAX)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRasConnections);
};

class CategoryOpenConnections : public CategoryInfo
{
public:
    CategoryOpenConnections()
        : CategoryInfo(system_info::network::kOpenConnections,
                       "Open Connections",
                       IDI_SERVERS_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        system_info::OpenConnections message;

        for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::TCP);
             !enumerator.IsAtEnd();
             enumerator.Advance())
        {
            system_info::OpenConnections::Item* item = message.add_item();

            item->set_process_name(enumerator.GetProcessName());
            item->set_local_address(enumerator.GetLocalAddress());
            item->set_remote_address(enumerator.GetRemoteAddress());
            item->set_local_port(enumerator.GetLocalPort());
            item->set_remote_port(enumerator.GetRemotePort());
            item->set_state(enumerator.GetState());
        }

        for (OpenConnectionEnumerator enumerator(OpenConnectionEnumerator::Type::UDP);
             !enumerator.IsAtEnd();
             enumerator.Advance())
        {
            system_info::OpenConnections::Item* item = message.add_item();

            item->set_process_name(enumerator.GetProcessName());
            item->set_local_address(enumerator.GetLocalAddress());
            item->set_remote_address(enumerator.GetRemoteAddress());
            item->set_local_port(enumerator.GetLocalPort());
            item->set_remote_port(enumerator.GetRemotePort());
            item->set_state(enumerator.GetState());
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenConnections);
};

class CategorySharedResources : public CategoryInfo
{
public:
    CategorySharedResources()
        : CategoryInfo(system_info::network::kSharedResources,
                       "Shared Resources",
                       IDI_FOLDER_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Name", 120);
        column_list->emplace_back("Type", 70);
        column_list->emplace_back("Description", 100);
        column_list->emplace_back("Local Path", 150);
        column_list->emplace_back("Current Uses", 100);
        column_list->emplace_back("Maximum Uses", 100);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::SharedResources message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::SharedResources::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.name());
            output->AddValue(TypeToString(item.type()));
            output->AddValue(item.description());
            output->AddValue(item.local_path());
            output->AddValue(std::to_string(item.current_uses()));
            output->AddValue(std::to_string(item.maximum_uses()));
        }
    }

    std::string Serialize() final
    {
        system_info::SharedResources message;

        for (ShareEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::SharedResources::Item* item = message.add_item();

            item->set_name(enumerator.GetName());

            switch (enumerator.GetType())
            {
                case ShareEnumerator::Type::DISK:
                    item->set_type(system_info::SharedResources::Item::TYPE_DISK);
                    break;

                case ShareEnumerator::Type::PRINTER:
                    item->set_type(system_info::SharedResources::Item::TYPE_PRINTER);
                    break;

                case ShareEnumerator::Type::DEVICE:
                    item->set_type(system_info::SharedResources::Item::TYPE_DEVICE);
                    break;

                case ShareEnumerator::Type::IPC:
                    item->set_type(system_info::SharedResources::Item::TYPE_IPC);
                    break;

                case ShareEnumerator::Type::SPECIAL:
                    item->set_type(system_info::SharedResources::Item::TYPE_SPECIAL);
                    break;

                case ShareEnumerator::Type::TEMPORARY:
                    item->set_type(system_info::SharedResources::Item::TYPE_TEMPORARY);
                    break;

                default:
                    item->set_type(system_info::SharedResources::Item::TYPE_UNKNOWN);
                    break;
            }

            item->set_local_path(enumerator.GetLocalPath());
            item->set_current_uses(enumerator.GetCurrentUses());
            item->set_maximum_uses(enumerator.GetMaximumUses());
        }

        return message.SerializeAsString();
    }

private:
    static const char* TypeToString(system_info::SharedResources::Item::Type type)
    {
        switch (type)
        {
            case system_info::SharedResources::Item::TYPE_DISK:
                return "Disk";

            case system_info::SharedResources::Item::TYPE_PRINTER:
                return "Printer";

            case system_info::SharedResources::Item::TYPE_DEVICE:
                return "Device";

            case system_info::SharedResources::Item::TYPE_IPC:
                return "IPC";

            case system_info::SharedResources::Item::TYPE_SPECIAL:
                return "Special";

            case system_info::SharedResources::Item::TYPE_TEMPORARY:
                return "Temporary";

            default:
                return "Unknown";
        }
    }

    DISALLOW_COPY_AND_ASSIGN(CategorySharedResources);
};

class CategoryOpenFiles : public CategoryInfo
{
public:
    CategoryOpenFiles()
        : CategoryInfo(system_info::network::kOpenFiles,
                       "Open Files",
                       IDI_FOLDER_NETWORK)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Parameter", 200);
        column_list->emplace_back("Value", 200);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        // TODO
    }

    std::string Serialize() final
    {
        // TODO
        return std::string();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryOpenFiles);
};

class CategoryRoutes : public CategoryInfo
{
public:
    CategoryRoutes() : CategoryInfo(system_info::network::kRoutes, "Routes", IDI_ROUTE)
    {
        ColumnList* column_list = mutable_column_list();
        column_list->emplace_back("Destonation", 150);
        column_list->emplace_back("Mask", 150);
        column_list->emplace_back("Gateway", 150);
        column_list->emplace_back("Metric", 100);
    }

    void Parse(std::shared_ptr<OutputProxy> output, const std::string& data) final
    {
        system_info::Routes message;

        if (!message.ParseFromString(data))
            return;

        Output::Table table(output, Name(), column_list());

        for (int index = 0; index < message.item_size(); ++index)
        {
            const system_info::Routes::Item& item = message.item(index);

            Output::Row row(output, Icon());

            output->AddValue(item.destonation());
            output->AddValue(item.mask());
            output->AddValue(item.gateway());
            output->AddValue(std::to_string(item.metric()));
        }
    }

    std::string Serialize() final
    {
        system_info::Routes message;

        for (RouteEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
        {
            system_info::Routes::Item* item = message.add_item();

            item->set_destonation(enumerator.GetDestonation());
            item->set_mask(enumerator.GetMask());
            item->set_gateway(enumerator.GetGateway());
            item->set_metric(enumerator.GetMetric());
        }

        return message.SerializeAsString();
    }

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryRoutes);
};

CategoryGroupNetwork::CategoryGroupNetwork()
    : CategoryGroup("Network", IDI_NETWORK)
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
