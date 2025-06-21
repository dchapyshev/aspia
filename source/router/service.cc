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

#include "router/service.h"

#include "base/logging.h"
#include "router/migration_utils.h"
#include "router/service_constants.h"

namespace router {

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(kServiceName, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service start...";

    if (isMigrationNeeded())
        doMigration();

    server_ = new Server(this);
    if (!server_->start())
    {
        LOG(ERROR) << "Unable to start server. Service not started";
        QCoreApplication::quit();
        return;
    }

    LOG(INFO) << "Service started";
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    LOG(INFO) << "Service stop...";
    delete server_;
    LOG(INFO) << "Service stopped";
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void Service::onSessionEvent(base::SessionStatus /* event */, base::SessionId /* session_id */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Service::onPowerEvent(quint32 /* event */)
{
    // Nothing
}
#endif // defined(Q_OS_WINDOWS)

} // namespace router
