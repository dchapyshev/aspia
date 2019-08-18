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

#include "router/win/router_service.h"

#include "router/win/router_service_constants.h"

namespace router {

Service::Service()
    : base::win::Service(kRouterServiceName, base::MessageLoop::Type::ASIO)
{
    // Nothing
}

Service::~Service() = default;

void Service::onStart()
{
    // TODO
}

void Service::onStop()
{
    // TODO
}

void Service::onSessionEvent(
    base::win::SessionStatus /* event */, base::win::SessionId /* session_id */)
{
    // Nothing
}

} // namespace router
