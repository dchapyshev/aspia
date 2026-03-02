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

#include "base/service.h"
#include "base/session_id.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "host/system_settings.h"

class QFileSystemWatcher;
class QTimer;

namespace base {
class Location;
class TcpChannel;
class TcpServer;
} // namespace base

namespace common {
class HttpFileDownloader;
class UpdateChecker;
} // namespace common

namespace proto::user {
class RouterState;
} // proto::user

namespace proto::chat {
class Chat;
} // namespace proto::chat

namespace host {

class DesktopClient;
class DesktopManager;
class FileClient;
class RouterManager;
class SystemInfoClient;
class ChatClient;
class UserSession;

extern const char kHostServiceFileName[];
extern const char kHostServiceName[];
extern const char kHostServiceDisplayName[];
extern const char kHostServiceDescription[];

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
    void onChangeOneTimePassword();
    void onChangeOneTimeSessions(quint32 sessions);
    void onNewDirectConnection();
    void onUserSessionAttached();
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onHostIdAssigned(base::HostId host_id);
    void onNewRelayConnection();
    void onConfirmationReply(quint32 request_id, bool accept);
    void onUpdateCheckedFinished(const QByteArray& result);
    void onFileDownloaderError(int error_code);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);
    void onRepeatedTasks();
    void onStopClient(quint32 client_id);

    void onDesktopManagerAttached();

    void onDesktopClientStarted(quint32 client_id);
    void onDesktopClientFinished(quint32 client_id);
    void onDesktopClientSwitchSession(base::SessionId session_id);

    void onFileClientStarted(quint32 client_id);
    void onFileClientFinished(quint32 client_id);

    void onSystemInfoClientStarted(quint32 client_id);
    void onSystemInfoClientFinished(quint32 client_id);

    void onChatClientStarted(quint32 client_id);
    void onChatClientFinished(quint32 client_id);
    void onChatClientMessage(quint32 client_id, const proto::chat::Chat& chat);
    void onUserChatMessage(const proto::chat::Chat& chat);

private:
    void startConfirmation(base::TcpChannel* tcp_channel);
    void startClient(base::TcpChannel* tcp_channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void updateConfiguration(const QString& path);
    void reloadUserList();
    void connectToRouter(const base::Location& location);
    void disconnectFromRouter(const base::Location& location);
    void checkForUpdates();

    void updateOneTimeCredentials(const base::Location& location);
    base::User createOneTimeUser() const;

    QTimer* repeated_timer_ = nullptr;

    QFileSystemWatcher* settings_watcher_ = nullptr;
    SystemSettings settings_;

    RouterManager* router_manager_ = nullptr;
    base::TcpServer* tcp_server_ = nullptr;

    DesktopManager* desktop_manager_ = nullptr;
    UserSession* user_session_ = nullptr;

    QList<std::pair<base::TcpChannel*, QTime>> pending_confirmation_;

    QList<DesktopClient*> desktop_clients_;
    QList<FileClient*> file_clients_;
    QList<SystemInfoClient*> system_info_clients_;
    QList<ChatClient*> chat_clients_;

    common::UpdateChecker* update_checker_ = nullptr;
    common::HttpFileDownloader* update_downloader_ = nullptr;

    QTimer* password_expire_timer_ = nullptr;
    QString one_time_password_;
    quint32 one_time_sessions_ = 0;

    Q_DISABLE_COPY_MOVE(Service)
};

} // namespace host

#endif // HOST_SERVICE_H
