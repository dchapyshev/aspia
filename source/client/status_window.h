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

#ifndef CLIENT_STATUS_WINDOW_H
#define CLIENT_STATUS_WINDOW_H

#include "base/peer/client_authenticator.h"
#include "client/router_controller.h"

namespace client {

class StatusWindow
{
public:
    virtual ~StatusWindow() = default;

    virtual void onStarted() = 0;
    virtual void onStopped() = 0;
    virtual void onRouterConnecting() = 0;
    virtual void onRouterConnected() = 0;
    virtual void onHostConnecting() = 0;
    virtual void onHostConnected() = 0;
    virtual void onHostDisconnected(base::NetworkChannel::ErrorCode error_code) = 0;
    virtual void onWaitForRouter() = 0;
    virtual void onWaitForRouterTimeout() = 0;
    virtual void onWaitForHost() = 0;
    virtual void onWaitForHostTimeout() = 0;
    virtual void onVersionMismatch() = 0;
    virtual void onAccessDenied(base::ClientAuthenticator::ErrorCode error_code) = 0;
    virtual void onRouterError(const RouterController::Error& error) = 0;
};

} // namespace client

#endif // CLIENT_STATUS_WINDOW_H
