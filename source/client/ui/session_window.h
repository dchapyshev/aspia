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

#ifndef CLIENT_UI_SESSION_WINDOW_H
#define CLIENT_UI_SESSION_WINDOW_H

#include "base/net/tcp_channel.h"
#include "client/client.h"
#include "client/client_config.h"
#include "client/router_controller.h"
#include "client/client_session_state.h"

#include <QWidget>

namespace common {
class StatusDialog;
} // namespace common

namespace client {

class SessionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SessionWindow(std::shared_ptr<SessionState> session_state, QWidget* parent = nullptr);
    virtual ~SessionWindow() override;

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(Config config);

    std::shared_ptr<SessionState> sessionState() { return session_state_; }

signals:
    void sig_start();
    void sig_stop();

protected:
    virtual Client* createClient() = 0;
    virtual void onInternalReset() = 0;

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;

public slots:
    void onStatusChanged(Client::Status status, const QVariant& data);

private:
    void setClientTitle(const Config& config);
    void onErrorOccurred(const QString& message);

    static QString netErrorToString(base::TcpChannel::ErrorCode error_code);
    static QString routerErrorToString(RouterController::ErrorCode error_code);

    std::shared_ptr<SessionState> session_state_;
    common::StatusDialog* status_dialog_ = nullptr;
};

} // namespace client

#endif // CLIENT_UI_SESSION_WINDOW_H
