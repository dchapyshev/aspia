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

#ifndef CLIENT__UI__CLIENT_WINDOW_H
#define CLIENT__UI__CLIENT_WINDOW_H

#include "client/client_config.h"
#include "client/status_window.h"

#include <QWidget>

namespace base {
class TaskRunner;
} // namespace base

namespace client {

class Client;
class StatusDialog;

class ClientWindow
    : public QWidget,
      public StatusWindow
{
    Q_OBJECT

public:
    explicit ClientWindow(QWidget* parent = nullptr);
    virtual ~ClientWindow();

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(Config config);

    Config config() const;

protected:
    virtual std::unique_ptr<Client> createClient(
        std::shared_ptr<base::TaskRunner> ui_task_runner) = 0;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

    // StatusWindow implementation.
    void onStarted(const std::u16string& address, uint16_t port) override;
    void onStopped() override;
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onAccessDenied(Authenticator::ErrorCode error_code) override;

private:
    void setClientTitle(const Config& config);
    void onErrorOccurred(const QString& message);

    std::unique_ptr<Client> client_;
    StatusDialog* status_dialog_ = nullptr;
};

} // namespace client

#endif // CLIENT__UI__CLIENT_WINDOW_H
