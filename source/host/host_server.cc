//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_server.h"

#include "base/guid.h"
#include "net/firewall_manager.h"
#include "net/network_channel.h"
#include "qt_base/qt_logging.h"

#include <QFileSystemWatcher>

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

} // namespace

HostServer::HostServer()
{

}

HostServer::~HostServer()
{
    
}

void HostServer::start()
{

}

void HostServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    
}

} // namespace host
