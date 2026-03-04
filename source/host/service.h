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

class Service final : public base::Service
{
    Q_OBJECT

public:
    explicit Service(QObject* parent = nullptr);
    ~Service() final;

    static const char kFileName[];
    static const char kName[];
    static const char kDisplayName[];
    static const char kDescription[];

protected:
    // base::Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onPowerEvent(quint32 power_event);
    void onNewDirectConnection();
    void onNewRelayConnection();
    void onConfirmationReply(quint32 request_id, bool accept);
    void onUpdateCheckedFinished(const QByteArray& result);
    void onFileDownloaderError(int error_code);
    void onFileDownloaderCompleted();
    void onFileDownloaderProgress(int percentage);
    void onRepeatedTasks();
    void onUserSessionAttached();
    void onStopClient(quint32 client_id);
    void onDesktopManagerAttached();
    void onClientFinished();
    void onChatClientStarted();
    void onChatClientFinished();
    void onChatClientMessage(quint32 client_id, const proto::chat::Chat& chat);
    void onUserChatMessage(const proto::chat::Chat& chat);
    void onSettingsChanged(const QString& path);

private:
    void startConfirmation(base::TcpChannel* tcp_channel);
    void startClient(base::TcpChannel* tcp_channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void reloadUserList();
    void connectToRouter(const base::Location& location);
    void disconnectFromRouter(const base::Location& location);
    void checkForUpdates();

    QTimer* repeated_timer_ = nullptr;

    QFileSystemWatcher* settings_watcher_ = nullptr;
    SystemSettings settings_;

    RouterManager* router_manager_ = nullptr;
    base::TcpServer* tcp_server_ = nullptr;

    DesktopManager* desktop_manager_ = nullptr;
    UserSession* user_session_ = nullptr;

    QList<std::pair<base::TcpChannel*, QTime>> pending_confirmation_;
    QList<QObject*> clients_;

    common::UpdateChecker* update_checker_ = nullptr;
    common::HttpFileDownloader* update_downloader_ = nullptr;

    Q_DISABLE_COPY_MOVE(Service)
};

} // namespace host

#endif // HOST_SERVICE_H
