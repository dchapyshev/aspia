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

#ifndef HOST_WIN_PORTABLE_USER_SESSION_H
#define HOST_WIN_PORTABLE_USER_SESSION_H

#include <QObject>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "base/session_id.h"
#include "base/peer/host_id.h"
#include "host/host_constants.h"
#include "proto/user.h"

class IpcChannel;
class IpcServer;
class SecureString;

class PortableUserSession final : public QObject
{
    Q_OBJECT

public:
    // |gui_session_id| is the session the GUI must run in, kInvalidSessionId accepts any session.
    PortableUserSession(const QString& ipc_channel_id, SessionId gui_session_id, QObject* parent = nullptr);
    ~PortableUserSession() final;

    SessionId sessionId() const { return session_id_; }

    void sendConnectEvent(quint32 client_id, proto::peer::SessionType session_type,
        const QString& computer_name, const QString& display_name);
    void sendDisconnectEvent(quint32 client_id);

public slots:
    bool start();
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onUpdateCredentials(HostId host_id, const SecureString& password);
    void onClientStarted();
    void onClientFinished();
    void onClientMessage(quint8 net_channel_id, const QByteArray& buffer);

signals:
    void sig_attached();
    void sig_guiTerminated();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);
    void sig_stopClient(quint32 client_id);
    void sig_lockMouseChanged(bool enable);
    void sig_lockKeyboardChanged(bool enable);
    void sig_pauseChanged(bool enable);
    void sig_userMessage(quint8 net_channel_id, const QByteArray& buffer);

private slots:
    void onIpcNewConnection();
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);

private:
    // Ends the session together with the GUI: the host terminates after it.
    void terminate();
    void sendMessage();

    const QString ipc_channel_id_;
    const SessionId gui_session_id_;

    IpcServer* ipc_server_ = nullptr;
    ScopedQPointer<IpcChannel> ipc_channel_;

    SessionId session_id_ = kInvalidSessionId;

    Parser<proto::user::UserToService> incoming_message_;
    Serializer<proto::user::ServiceToUser> outgoing_message_;

    Q_DISABLE_COPY_MOVE(PortableUserSession)
};

#endif // HOST_WIN_PORTABLE_USER_SESSION_H
