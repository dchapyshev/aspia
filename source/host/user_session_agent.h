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

#ifndef HOST_USER_SESSION_AGENT_H
#define HOST_USER_SESSION_AGENT_H

#include <QObject>
#include <QVector>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "common/clipboard.h"
#include "proto/user.h"

class ClipboardFileTransfer;
class IpcChannel;

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

class UserSessionAgent final : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        CONNECTED_TO_SERVICE,
        DISCONNECTED_FROM_SERVICE,
        SERVICE_NOT_AVAILABLE
    };
    Q_ENUM(Status)

    struct Client
    {
        explicit Client(const proto::user::ConnectEvent& event)
            : client_id(event.client_id()),
              computer_name(QString::fromStdString(event.computer_name())),
              display_name(QString::fromStdString(event.display_name())),
              session_type(event.session_type())
        {
            // Nothing
        }

        quint32 client_id;
        QString computer_name;
        QString display_name;
        proto::peer::SessionType session_type;
    };

    using ClientList = QVector<Client>;

    explicit UserSessionAgent(QObject* parent = nullptr);
    ~UserSessionAgent() final;

public slots:
    void onConnectToService();
    void onUpdateCredentials(proto::user::CredentialsRequest_Type type);
    void onOneTimeSessions(quint32 sessions);
    void onStopClient(quint32 client_id);
    void onConnectConfirmation(quint32 request_id, bool accept);
    void onMouseLock(bool enable);
    void onKeyboardLock(bool enable);
    void onPause(bool enable);
    void onChat(const proto::chat::Chat& chat);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onClipboardLocalFileListChanged(const QVector<LocalFileEntry>& files);
    void onClipboardFileDataRequest(int file_index);

signals:
    void sig_statusChanged(UserSessionAgent::Status status);
    void sig_clientListChanged(const UserSessionAgent::ClientList& clients);
    void sig_credentialsChanged(const proto::user::Credentials& credentials);
    void sig_routerStateChanged(const proto::user::RouterState& state);
    void sig_confirmationRequest(const proto::user::ConfirmationRequest& request);
    void sig_recordingStateChanged(bool started);
    void sig_chat(const proto::chat::Chat& chat);
    void sig_injectClipboardEvent(const proto::clipboard::Event& event);
    void sig_clipboardFileData(int file_index, const QByteArray& data, bool is_last);

private slots:
    void onIpcConnected();
    void onIpcDisconnected();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);
    void onConnectEvent(const proto::user::ConnectEvent& event);
    void onDisconnectEvent(const proto::user::DisconnectEvent& event);

private:
    void sendServiceMessage();
    void sendNetworkMessage(quint8 net_channel_id, const QByteArray& buffer);

    ScopedQPointer<ClipboardFileTransfer> clipboard_file_transfer_;
    ScopedQPointer<IpcChannel> ipc_channel_;

    Parser<proto::user::ServiceToUser> incoming_message_;
    Serializer<proto::user::UserToService> outgoing_message_;

    ClientList clients_;

    Q_DISABLE_COPY_MOVE(UserSessionAgent)
};

Q_DECLARE_METATYPE(UserSessionAgent::Status)
Q_DECLARE_METATYPE(UserSessionAgent::ClientList)

#endif // HOST_USER_SESSION_AGENT_H
