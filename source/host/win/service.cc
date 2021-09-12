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
#include "base/strings/string_printf.h"
#include "base/win/safe_mode_util.h"
#include "base/win/session_status.h"
#include "host/win/service_constants.h"
#include "host/server.h"

#include <Windows.h>

namespace host {

namespace {

std::string powerEventToString(uint32_t event)
{
    const char* name;

    switch (event)
    {
        case PBT_APMPOWERSTATUSCHANGE:
            name = "PBT_APMPOWERSTATUSCHANGE";
            break;

        case PBT_APMRESUMEAUTOMATIC:
            name = "PBT_APMRESUMEAUTOMATIC";
            break;

        case PBT_APMRESUMESUSPEND:
            name = "PBT_APMRESUMESUSPEND";
            break;

        case PBT_APMSUSPEND:
            name = "PBT_APMSUSPEND";
            break;

        case PBT_POWERSETTINGCHANGE:
            name = "PBT_POWERSETTINGCHANGE";
            break;

        default:
            name = "UNKNOWN";
            break;
    }

    return base::stringPrintf("%s (%d)", name, static_cast<int>(event));
}

} // namespace

Service::Service()
    : base::win::Service(kHostServiceName, base::MessageLoop::Type::ASIO)
{
    LOG(LS_INFO) << "Service Ctor";
}

Service::~Service()
{
    LOG(LS_INFO) << "Service Dtor";
}

void Service::onStart()
{
    LOG(LS_INFO) << "Service is started";

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(LS_WARNING) << "SetPriorityClass failed";
    }

    if (!base::win::SafeModeUtil::setSafeMode(false))
    {
        LOG(LS_WARNING) << "Failed to turn off boot in safe mode";
    }

    if (!base::win::SafeModeUtil::setSafeModeService(kHostServiceName, false))
    {
        LOG(LS_WARNING) << "Failed to remove service from boot in Safe Mode";
    }

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

void Service::onPowerEvent(uint32_t event)
{
    LOG(LS_INFO) << "Power event detected: " << powerEventToString(event);

    if (server_)
        server_->setPowerEvent(event);
}

} // namespace host
