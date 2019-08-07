//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/host_service.h"

#include "base/logging.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/security_helpers.h"
#include "host/win/host_service_constants.h"
#include "host/host_server.h"
#include "host/host_settings.h"

#include <Windows.h>
#include <sddl.h>

namespace host {

namespace {

// Concatenates ACE type, permissions and sid given as SDDL strings into an ACE
// definition in SDDL form.
#define SDDL_ACE(type, permissions, sid) L"(" type L";;" permissions L";;;" sid L")"

// Text representation of COM_RIGHTS_EXECUTE and COM_RIGHTS_EXECUTE_LOCAL
// permission bits that is used in the SDDL definition below.
#define SDDL_COM_EXECUTE_LOCAL L"0x3"

// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
const wchar_t kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

// Appended to |kComProcessSd| to specify that only callers running at medium
// or higher integrity level are allowed to call COM methods exposed by the
// daemon.
const wchar_t kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

} // namespace

HostService::HostService()
    : Service<QCoreApplication>(QLatin1String(kHostServiceName),
                                QLatin1String(kHostServiceDisplayName),
                                QLatin1String(kHostServiceDescription))
{
    // Nothing
}

HostService::~HostService() = default;

void HostService::start()
{
    LOG(LS_INFO) << "Service is started";

    com_initializer_.reset(new base::win::ScopedCOMInitializer());
    if (!com_initializer_->isSucceeded())
    {
        LOG(LS_FATAL) << "COM not initialized";
        return;
    }

    base::win::initializeComSecurity(kComProcessSd, kComProcessMandatoryLabel, false);

    server_.reset(new HostServer());
    server_->start();
}

void HostService::stop()
{
    server_.reset();
    com_initializer_.reset();

    LOG(LS_INFO) << "Service is stopped";
}

void HostService::sessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    if (server_)
        server_->setSessionEvent(status, session_id);
}

} // namespace host
