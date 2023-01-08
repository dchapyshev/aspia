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

#ifndef CLIENT_UI_SESSION_WINDOW_H
#define CLIENT_UI_SESSION_WINDOW_H

#include "client/client_config.h"
#include "client/status_window.h"

#include <QWidget>

namespace base {
class TaskRunner;
} // namespace base

namespace common {
class StatusDialog;
} // namespace common

namespace client {

class Client;
class ClientProxy;
class StatusWindowProxy;

class SessionWindow
    : public QWidget,
      public StatusWindow
{
    Q_OBJECT

public:
    explicit SessionWindow(QWidget* parent = nullptr);
    virtual ~SessionWindow() override;

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(Config config);

    Config config() const;

protected:
    virtual std::unique_ptr<Client> createClient() = 0;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

    // StatusWindow implementation.
    void onStarted(const std::u16string& address_or_id) override;
    void onStopped() override;
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onAccessDenied(base::ClientAuthenticator::ErrorCode error_code) override;
    void onRouterError(const RouterController::Error& error) override;

private:
    void setClientTitle(const Config& config);
    void onErrorOccurred(const QString& message);

    static QString netErrorToString(base::NetworkChannel::ErrorCode error_code);
    static QString authErrorToString(base::ClientAuthenticator::ErrorCode error_code);
    static QString routerErrorToString(RouterController::ErrorCode error_code);

    std::shared_ptr<StatusWindowProxy> status_window_proxy_;
    std::unique_ptr<ClientProxy> client_proxy_;
    common::StatusDialog* status_dialog_ = nullptr;
};

} // namespace client

#endif // CLIENT_UI_SESSION_WINDOW_H
