//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/service.h"

#include "base/logging.h"
#include "base/win/session_status.h"
#include "host/win/service_constants.h"
#include "host/server.h"

#include <Windows.h>

namespace host {

Service::Service()
    : base::win::Service(kHostServiceName, base::MessageLoop::Type::ASIO)
{
    // Nothing
}

Service::~Service() = default;

void Service::onStart()
{
    LOG(LS_INFO) << "Service is started";

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    server_ = std::make_unique<Server>(taskRunner());
    server_->start();
}

void Service::onStop()
{
    LOG(LS_INFO) << "Service stopping...";
    server_.reset();
    LOG(LS_INFO) << "Service is stopped";
}

void Service::onSessionEvent(base::win::SessionStatus status, base::SessionId session_id)
{
    LOG(LS_INFO) << "Session event detected (status: " << base::win::sessionStatusToString(status)
                 << ", session_id: " << session_id << ")";

    if (server_)
        server_->setSessionEvent(status, session_id);
}

} // namespace host
