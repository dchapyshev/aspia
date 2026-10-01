//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/win/portable_service.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/net/firewall_manager.h"
#include "host/win/portable_host.h"

namespace {

const char kFirewallRuleDescription[] = "Allow incoming connections for Aspia Quick Support";

//--------------------------------------------------------------------------------------------------
QString firewallTcpRuleName()
{
    return PortableHost::serviceName() + " (TCP)";
}

//--------------------------------------------------------------------------------------------------
QString firewallUdpRuleName()
{
    return PortableHost::serviceName() + " (UDP)";
}

} // namespace

//--------------------------------------------------------------------------------------------------
PortableService::PortableService(QObject* parent)
    : CoreService(PortableHost::serviceName(), parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableService::~PortableService()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableService::onStart()
{
    LOG(INFO) << "Portable service is started";

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    if (!firewall.addTcpRule(firewallTcpRuleName(), kFirewallRuleDescription))
        LOG(ERROR) << "Unable to add firewall rule for TCP";

    if (!firewall.addUdpRule(firewallUdpRuleName(), kFirewallRuleDescription))
        LOG(ERROR) << "Unable to add firewall rule for UDP";
}

//--------------------------------------------------------------------------------------------------
void PortableService::onStop()
{
    LOG(INFO) << "Portable service is stopped";

    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    firewall.deleteRuleByName(firewallTcpRuleName());
    firewall.deleteRuleByName(firewallUdpRuleName());
}
