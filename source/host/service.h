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

#ifndef HOST_SERVICE_H
#define HOST_SERVICE_H

#include <QFileSystemWatcher>
#include <QPointer>
#include <QTimer>

#include "base/service.h"
#include "base/net/tcp_server.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "host/router_controller.h"
#include "host/user_session_manager.h"
#include "host/system_settings.h"

namespace host {

class Service final : public base::Service
{
    Q_OBJECT

public:
    explicit Service(QObject* parent = nullptr);
    ~Service() final;

protected:
    // base::Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onPowerEvent(quint32 power_event);
    void onRouterStateRequested();
    void onCredentialsRequested();
    void onChangeOneTimePassword();
    void onChangeOneTimeSessions(quint32 sessions);
    void onNewDirectConnection();
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onHostIdAssigned(base::HostId host_id);
    void onNewRelayConnection();
    void onUpdateCheckedFinished(const QByteArray& result);
    void onFileDownloaderError(int error_code);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);

private:
    void startSession(base::TcpChannel* channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void updateConfiguration(const QString& path);
    void reloadUserList();
    void connectToRouter();
    void disconnectFromRouter();
    void checkForUpdates();

    void updateOneTimeCredentials(const base::Location& location);
    base::User createOneTimeUser() const;

    QTimer* update_timer_ = nullptr;

    QFileSystemWatcher* settings_watcher_ = nullptr;
    SystemSettings settings_;

    QPointer<RouterController> router_controller_;

    base::TcpServer* tcp_server_ = nullptr;
    UserSessionManager* user_session_manager_ = nullptr;

    QPointer<common::UpdateChecker> update_checker_;
    QPointer<common::HttpFileDownloader> update_downloader_;

    QTimer* password_expire_timer_ = nullptr;
    QString one_time_password_;
    quint32 one_time_sessions_ = 0;

    Q_DISABLE_COPY(Service)
};

} // namespace host

#endif // HOST_SERVICE_H
