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

#include "host/workers/portable_service_worker.h"

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"
#include "host/router_config_provider.h"
#include "host/router_manager.h"
#include "proto/user.h"

//--------------------------------------------------------------------------------------------------
PortableServiceWorker::PortableServiceWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableServiceWorker::~PortableServiceWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onPrepare()
{
    router_manager_ = new RouterManager(std::make_unique<PortableConfigProvider>(), this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            this, &PortableServiceWorker::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            this, &PortableServiceWorker::onCredentialsChanged);
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onStart()
{
    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onStop()
{
    router_manager_.reset();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onRouterStateChanged(const proto::user::RouterState& state)
{
    LOG(INFO) << "Router state changed:" << state.state();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onCredentialsChanged(HostId host_id, const SecureString& /* one_time_password */)
{
    LOG(INFO) << "Credentials changed. Host ID:" << host_id << "(temporary:" << isTempHostId(host_id) << ")";
}
