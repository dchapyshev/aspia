//
// PROJECT:         Aspia
// FILE:            category/category_network_card.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "network/network_adapter_enumerator.h"
#include "category/category_network_card.h"
#include "category/category_network_card.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryNetworkCard::CategoryNetworkCard()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryNetworkCard::Name() const
{
    return "Network Cards";
}

Category::IconId CategoryNetworkCard::Icon() const
{
    return IDI_NETWORK_ADAPTER;
}

const char* CategoryNetworkCard::Guid() const
{
    return "{98D665E9-0F78-4054-BDF3-A51E950A8618}";
}

void CategoryNetworkCard::Parse(Table& table, const std::string& data)
{
    proto::NetworkCard message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::NetworkCard::Item& item = message.item(index);

        Group group = table.AddGroup(item.adapter_name());

        group.AddParam("Connection Name", Value::String(item.connection_name()));
        group.AddParam("Interface Type", Value::String(item.interface_type()));
        group.AddParam("MTU", Value::Number(item.mtu(), "byte"));
        group.AddParam("Connection Speed", Value::Number(item.speed() / (1000 * 1000), "Mbps"));
        group.AddParam("MAC Address", Value::String(item.mac_address()));

        if (item.ip_address_size())
        {
            Group addr_group = group.AddGroup("IP Addresses");

            for (int i = 0; i < item.ip_address_size(); ++i)
            {
                const proto::NetworkCard::Item::IpAddress& address =
                    item.ip_address(i);

                addr_group.AddParam(StringPrintf("Address #%d", i + 1),
                                    Value::String(address.address()));

                addr_group.AddParam(StringPrintf("Mask #%d", i + 1),
                                    Value::String(address.mask()));
            }
        }

        if (item.gateway_address_size())
        {
            Group addr_group = group.AddGroup("Gateway");

            for (int i = 0; i < item.gateway_address_size(); ++i)
            {
                addr_group.AddParam(StringPrintf("Address #%d", i + 1),
                                    Value::String(item.gateway_address(i)));
            }
        }

        group.AddParam("DHCP Enabled", Value::Bool(item.is_dhcp_enabled()));

        if (item.is_dhcp_enabled() && item.dhcp_server_address_size())
        {
            Group addr_group = group.AddGroup("DHCP Server");

            for (int i = 0; i < item.dhcp_server_address_size(); ++i)
            {
                addr_group.AddParam(StringPrintf("Address #%d", i + 1),
                                    Value::String(item.dhcp_server_address(i)));
            }
        }

        if (item.dns_server_address_size())
        {
            Group addr_group = group.AddGroup("DNS Server");

            for (int i = 0; i < item.dns_server_address_size(); ++i)
            {
                addr_group.AddParam(StringPrintf("Address #%d", i + 1),
                                    Value::String(item.dns_server_address(i)));
            }
        }

        group.AddParam("WINS Enabled", Value::Bool(item.is_wins_enabled()));

        if (item.is_wins_enabled())
        {
            group.AddParam("Primary WINS Server", Value::String(item.primary_wins_server()));
            group.AddParam("Secondary WINS Server", Value::String(item.secondary_wins_server()));
        }
    }
}

std::string CategoryNetworkCard::Serialize()
{
    proto::NetworkCard message;

    for (NetworkAdapterEnumerator enumerator; !enumerator.IsAtEnd(); enumerator.Advance())
    {
        proto::NetworkCard::Item* item = message.add_item();

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
            proto::NetworkCard::Item::IpAddress* address = item->add_ip_address();

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

} // namespace aspia
