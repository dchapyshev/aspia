//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/service.h"

#include "base/logging.h"
#include "base/win/safe_mode_util.h"
#include "base/win/session_status.h"
#include "host/service_constants.h"
#include "host/server.h"

#include <fmt/format.h>

#if defined(OS_WIN)
#include <Windows.h>
#endif // defined(OS_WIN)

namespace host {

namespace {

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
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

    return fmt::format("{} ({})", name, static_cast<int>(event));
}
#endif // defined(OS_WIN)

} // namespace

//--------------------------------------------------------------------------------------------------
Service::Service()
    : base::Service(kHostServiceName)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(LS_INFO) << "Service is started";

#if defined(OS_WIN)
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        PLOG(LS_ERROR) << "SetPriorityClass failed";
    }

    SystemSettings settings;
    if (settings.isBootToSafeMode())
    {
        settings.setBootToSafeMode(false);
        settings.sync();

        if (!base::SafeModeUtil::setSafeMode(false))
        {
            LOG(LS_ERROR) << "Failed to turn off boot in safe mode";
        }
        else
        {
            LOG(LS_INFO) << "Safe mode is disabled";
        }

        if (!base::SafeModeUtil::setSafeModeService(kHostServiceName, false))
        {
            LOG(LS_ERROR) << "Failed to remove service from boot in Safe Mode";
        }
        else
        {
            LOG(LS_INFO) << "Service removed from safe mode loading";
        }
    }
#endif // defined(OS_WIN)

    server_ = std::make_unique<Server>(taskRunner());
    server_->start();
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    LOG(LS_INFO) << "Service stopping...";
    server_.reset();
    LOG(LS_INFO) << "Service is stopped";
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void Service::onSessionEvent(base::SessionStatus status, base::SessionId session_id)
{
    LOG(LS_INFO) << "Session event detected (status: " << base::sessionStatusToString(status)
                 << ", session_id: " << session_id << ")";

    if (server_)
    {
        server_->setSessionEvent(status, session_id);
    }
    else
    {
        LOG(LS_ERROR) << "No server instance";
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onPowerEvent(uint32_t event)
{
    LOG(LS_INFO) << "Power event detected: " << powerEventToString(event);

    if (server_)
    {
        server_->setPowerEvent(event);
    }
    else
    {
        LOG(LS_ERROR) << "No server instance";
    }
}
#endif // defined(OS_WIN)

} // namespace host
