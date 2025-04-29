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

#include "base/session_id.h"
#include "base/version.h"
#include "base/net/tcp_channel.h"
#include "proto/desktop_extensions.h"
#include "proto/text_chat.h"

#include <QObject>

namespace base {
class TcpChannelProxy;
class TaskRunner;
} // namespace base

namespace host {

class DesktopSessionProxy;

class ClientSession : public QObject
{
    Q_OBJECT

public:
    virtual ~ClientSession() override;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onClientSessionConfigured() = 0;
        virtual void onClientSessionFinished() = 0;
        virtual void onClientSessionVideoRecording(
            const QString& computer_name, const QString& user_name, bool started) = 0;
        virtual void onClientSessionTextChat(uint32_t id, const proto::TextChat& text_chat) = 0;
    };

    enum class State
    {
        CREATED, // Session is created but not yet started.
        STARTED, // Session is started.
        FINISHED // Session is stopped.
    };

    static std::unique_ptr<ClientSession> create(proto::SessionType session_type,
                                                 std::unique_ptr<base::TcpChannel> channel,
                                                 QObject* parent = nullptr);

    void start(Delegate* delegate);
    void stop();

    State state() const { return state_; }
    uint32_t id() const { return id_; }

    void setClientVersion(const base::Version& version);
    const base::Version& clientVersion() const { return version_; }

    void setUserName(const QString& username);
    const QString& userName() const { return username_; }

    void setComputerName(const QString& computer_name);
    const QString& computerName() const;

    void setDisplayName(const QString& display_name);
    const QString& displayName() const;

    proto::SessionType sessionType() const { return session_type_; }

    void setSessionId(base::SessionId session_id);
    base::SessionId sessionId() const { return session_id_; }

    base::HostId hostId() const { return channel_->hostId(); }

protected:
    ClientSession(proto::SessionType session_type,
                  std::unique_ptr<base::TcpChannel> channel,
                  QObject* parent);

    // Called when the session is ready to send and receive data. When this method is called, the
    // session should start initializing (for example, making a configuration request).
    virtual void onStarted() = 0;
    virtual void onReceived(uint8_t channel_id, const QByteArray& buffer) = 0;
    virtual void onWritten(uint8_t channel_id, size_t pending) = 0;

    std::shared_ptr<base::TcpChannelProxy> channelProxy();
    void sendMessage(uint8_t channel_id, QByteArray&& buffer);
    void sendMessage(uint8_t channel_id, const google::protobuf::MessageLite& message);

    size_t pendingMessages() const;

    Delegate* delegate_ = nullptr;

private slots:
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer);
    void onTcpMessageWritten(uint8_t channel_id, size_t pending);

private:
    base::SessionId session_id_ = base::kInvalidSessionId;
    State state_ = State::CREATED;
    uint32_t id_;
    proto::SessionType session_type_;
    base::Version version_;
    QString username_;
    QString computer_name_;
    QString display_name_;

    std::unique_ptr<base::TcpChannel> channel_;
};

} // namespace host

#endif // HOST_CLIENT_SESSION_H
