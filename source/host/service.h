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

#include <QTime>

#include "base/scoped_qpointer.h"
#include "base/core_service.h"
#include "host/system_settings.h"

class HttpFileDownloader;
class Location;
class QFileSystemWatcher;
class QTimer;
class TcpChannel;
class TcpServer;
class UpdateChecker;

namespace proto::chat {
class Chat;
} // namespace proto::chat

class Client;
class DesktopManager;
class RouterManager;
class UserSession;

class Service final : public CoreService
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
    // Service implementation.
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
    void onUserSessionDettached();
    void onStopClient(quint32 client_id);
    void onDesktopManagerAttached();
    void onClientFinished();
    void onChatClientStarted();
    void onChatClientFinished();
    void onChatClientMessage(const proto::chat::Chat& chat);
    void onUserChatMessage(const proto::chat::Chat& chat);
    void onSettingsChanged(const QString& path);
    void onRemoveHost();

private:
    struct PendingConfirmation
    {
        TcpChannel* tcp_channel = nullptr;
        QTime start_time;
        QString stun_host;
        quint16 stun_port = 0;
        bool peer_equals = false;
        bool direct = false;
    };

    void startConfirmation(PendingConfirmation& pending);
    void startClient(const PendingConfirmation& pending);
    void addFirewallRules();
    void deleteFirewallRules();
    void connectToRouter(const Location& location);
    void disconnectFromRouter(const Location& location);
    void checkForUpdates();

    QTimer* repeated_timer_ = nullptr;

    QFileSystemWatcher* settings_watcher_ = nullptr;
    SystemSettings settings_;

    ScopedQPointer<RouterManager> router_manager_;
    TcpServer* tcp_server_ = nullptr;

    DesktopManager* desktop_manager_ = nullptr;
    UserSession* user_session_ = nullptr;

    QList<PendingConfirmation> pending_confirmation_;
    QList<Client*> clients_;

    ScopedQPointer<UpdateChecker> update_checker_;
    ScopedQPointer<HttpFileDownloader> update_downloader_;

    Q_DISABLE_COPY_MOVE(Service)
};

#endif // HOST_SERVICE_H
