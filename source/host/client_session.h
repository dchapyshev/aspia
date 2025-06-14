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

#ifndef HOST_CLIENT_SESSION_H
#define HOST_CLIENT_SESSION_H

#include <QObject>
#include <QVersionNumber>

#include "base/session_id.h"
#include "base/net/tcp_channel.h"
#include "proto/desktop.h"
#include "proto/peer.h"
#include "proto/text_chat.h"

namespace host {

class ClientSession : public QObject
{
    Q_OBJECT

public:
    virtual ~ClientSession() override;

    enum class State
    {
        CREATED, // Session is created but not yet started.
        STARTED, // Session is started.
        FINISHED // Session is stopped.
    };
    Q_ENUM(State)

    static ClientSession* create(
        proto::peer::SessionType session_type, base::TcpChannel* channel, QObject* parent = nullptr);

    void start();
    void stop();

    State state() const { return state_; }
    quint32 id() const { return id_; }

    void setClientVersion(const QVersionNumber& version);
    const QVersionNumber& clientVersion() const { return version_; }

    void setUserName(const QString& username);
    const QString& userName() const { return username_; }

    void setComputerName(const QString& computer_name);
    const QString& computerName() const;

    void setDisplayName(const QString& display_name);
    const QString& displayName() const;

    proto::peer::SessionType sessionType() const { return session_type_; }

    void setSessionId(base::SessionId session_id);
    base::SessionId sessionId() const { return session_id_; }

    base::HostId hostId() const { return tcp_channel_->hostId(); }

signals:
    void sig_clientSessionConfigured();
    void sig_clientSessionFinished();
    void sig_clientSessionVideoRecording(
        const QString& computer_name, const QString& user_name, bool started);
    void sig_clientSessionTextChat(quint32 id, const proto::text_chat::TextChat& text_chat);

protected:
    ClientSession(proto::peer::SessionType session_type, base::TcpChannel* channel, QObject* parent);

    // Called when the session is ready to send and receive data. When this method is called, the
    // session should start initializing (for example, making a configuration request).
    virtual void onStarted() = 0;
    virtual void onReceived(const QByteArray& buffer) = 0;

    void sendMessage(const QByteArray& buffer);
    size_t pendingMessages() const;

private slots:
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    base::SessionId session_id_ = base::kInvalidSessionId;
    State state_ = State::CREATED;
    quint32 id_;
    proto::peer::SessionType session_type_;
    QVersionNumber version_;
    QString username_;
    QString computer_name_;
    QString display_name_;

    base::TcpChannel* tcp_channel_ = nullptr;
};

} // namespace host

#endif // HOST_CLIENT_SESSION_H
