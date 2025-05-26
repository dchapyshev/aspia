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

#ifndef HOST_SERVER_H
#define HOST_SERVER_H

#include <QFileSystemWatcher>
#include <QPointer>
#include <QTimer>

#include "base/net/tcp_server.h"
#include "base/peer/server_authenticator_manager.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "host/router_controller.h"
#include "host/user_session_manager.h"
#include "host/system_settings.h"

namespace host {

class Server final : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);
    ~Server() final;

    void start();
    void setSessionEvent(base::SessionStatus status, base::SessionId session_id);
    void setPowerEvent(quint32 power_event);

private slots:
    void onHostIdRequest();
    void onResetHostId(base::HostId host_id);
    void onUserListChanged();
    void onSessionAuthenticated();
    void onNewDirectConnection();
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onHostIdAssigned(base::HostId host_id);
    void onNewRelayConnection();
    void onUpdateCheckedFinished(const QByteArray& result);
    void onFileDownloaderError(int error_code);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);

private:
    void startAuthentication(base::TcpChannel* channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void updateConfiguration(const QString& path);
    void reloadUserList();
    void connectToRouter();
    void disconnectFromRouter();
    void checkForUpdates();

    QTimer* update_timer_ = nullptr;

    QFileSystemWatcher* settings_watcher_ = nullptr;
    SystemSettings settings_;

    // Accepts incoming network connections.
    base::TcpServer* server_ = nullptr;
    QPointer<RouterController> router_controller_ = nullptr;
    base::ServerAuthenticatorManager* authenticator_manager_ = nullptr;
    UserSessionManager* user_session_manager_ = nullptr;

    common::UpdateChecker* update_checker_ = nullptr;
    common::HttpFileDownloader* update_downloader_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace host

#endif // HOST_SERVER_H
