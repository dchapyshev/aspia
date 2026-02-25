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

#ifndef HOST_USER_SESSION_MANAGER_H
#define HOST_USER_SESSION_MANAGER_H

#include <QObject>
#include <QList>

#include "base/serialization.h"
#include "base/session_id.h"
#include "base/peer/host_id.h"
#include "base/ipc/ipc_server.h"
#include "proto/host_internal.h"

namespace host {

class UserSessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit UserSessionManager(QObject* parent = nullptr);
    ~UserSessionManager() final;

    bool start();
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onUpdateCredentials(base::HostId host_id, const QString& password);
    bool isConnected() const;

public slots:
    void onAskForConfirmation(
        base::SessionId session_id, const proto::internal::ConnectConfirmationRequest& request);
    void onClientStarted(const proto::internal::ConnectEvent& event);
    void onClientFinished(const proto::internal::DisconnectEvent& event);
    void onClientSessionTextChat(quint32 client_id, const proto::text_chat::TextChat& text_chat);
    void onClientSessionVideoRecording(
        const QString& computer_name, const QString& user_name, bool started);

signals:
    void sig_attached();
    void sig_dettached();
    void sig_routerStateRequested();
    void sig_credentialsRequested();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);
    void sig_askForConfirmation(quint32 request_id, bool accept);
    void sig_stopClient(quint32 client_id);
    void sig_textChatMessage(const proto::text_chat::TextChat& text_chat);
    void sig_lockMouseChanged(bool enable);
    void sig_lockKeyboardChanged(bool enable);
    void sig_pauseChanged(bool enable);

private slots:
    void onUserSessionEvent(quint32 status, quint32 session_id);
    void onIpcNewConnection();
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer);

private:
    void attach(const base::Location& location, base::SessionId session_id);
    void dettach(const base::Location& location);
    void sendSessionMessage();

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;

    base::SessionId session_id_ = base::kInvalidSessionId;

    bool is_console_ = true;
    QTimer* attach_timer_ = nullptr;

    base::Parser<proto::internal::UiToService> incoming_message_;
    base::Serializer<proto::internal::ServiceToUi> outgoing_message_;

    Q_DISABLE_COPY_MOVE(UserSessionManager)
};

} // namespace host

#endif // HOST_USER_SESSION_MANAGER_H
