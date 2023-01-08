//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/service.h"

#include "base/logging.h"
#include "router/server.h"
#include "router/service_constants.h"

namespace router {

Service::Service()
    : base::Service(kServiceName, base::MessageLoop::Type::ASIO)
{
    LOG(LS_INFO) << "Ctor";
}

Service::~Service()
{
    LOG(LS_INFO) << "Dtor";
}

void Service::onStart()
{
    LOG(LS_INFO) << "Service start...";

    std::shared_ptr<base::TaskRunner> task_runner = taskRunner();
    DCHECK(task_runner);

    server_ = std::make_unique<Server>(task_runner); 
    if (!server_->start())
    {
        LOG(LS_WARNING) << "Unable to start server. Service not started";
        task_runner->postQuit();
        return;
    }

    LOG(LS_INFO) << "Service started";
}

void Service::onStop()
{
    LOG(LS_INFO) << "Service stop...";
    server_.reset();
    LOG(LS_INFO) << "Service stopped";
}

#if defined(OS_WIN)
void Service::onSessionEvent(
    base::win::SessionStatus /* event */, base::SessionId /* session_id */)
{
    // Nothing
}

void Service::onPowerEvent(uint32_t /* event */)
{
    // Nothing
}
#endif // defined(OS_WIN)

} // namespace router
