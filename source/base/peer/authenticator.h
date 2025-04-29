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

#ifndef BASE_PEER_AUTHENTICATOR_H
#define BASE_PEER_AUTHENTICATOR_H

#include "base/version.h"
#include "base/net/tcp_channel.h"
#include "proto/key_exchange.pb.h"

#include <QPointer>
#include <QTimer>

namespace base {

class Location;

class Authenticator : public QObject
{
    Q_OBJECT

public:
    explicit Authenticator(QObject* parent);
    virtual ~Authenticator() override;

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };

    enum class ErrorCode
    {
        SUCCESS,
        UNKNOWN_ERROR,
        NETWORK_ERROR,
        PROTOCOL_ERROR,
        VERSION_ERROR,
        ACCESS_DENIED,
        SESSION_DENIED
    };

    void start(TcpChannel* channel);

    [[nodiscard]] proto::Identify identify() const { return identify_; }
    [[nodiscard]] proto::Encryption encryption() const { return encryption_; }
    [[nodiscard]] const Version& peerVersion() const { return peer_version_; }
    [[nodiscard]] const QString& peerOsName() const { return peer_os_name_; }
    [[nodiscard]] const QString& peerComputerName() const { return peer_computer_name_; }
    [[nodiscard]] const QString& peerArch() const { return peer_arch_; }
    [[nodiscard]] const QString& peerDisplayName() const { return peer_display_name_; }
    [[nodiscard]] uint32_t sessionType() const { return session_type_; }
    [[nodiscard]] const QString& userName() const { return user_name_; }

    // Returns the current state.
    [[nodiscard]] State state() const { return state_; }

    static const char* stateToString(State state);
    static const char* errorToString(Authenticator::ErrorCode error_code);

signals:
    void sig_finished(ErrorCode error_code);

protected:
    [[nodiscard]] virtual bool onStarted() = 0;
    virtual void onReceived(const QByteArray& buffer) = 0;
    virtual void onWritten() = 0;

    void sendMessage(const google::protobuf::MessageLite& message);
    void sendMessage(QByteArray&& data);
    void finish(const Location& location, ErrorCode error_code);
    void setPeerVersion(const proto::Version& version);
    void setPeerOsName(const QString& name);
    void setPeerComputerName(const QString& name);
    void setPeerArch(const QString& arch);
    void setPeerDisplayName(const QString& display_name);

    [[nodiscard]] bool onSessionKeyChanged();

    proto::Encryption encryption_ = proto::ENCRYPTION_UNKNOWN;
    proto::Identify identify_ = proto::IDENTIFY_SRP;
    QByteArray session_key_;
    QByteArray encrypt_iv_;
    QByteArray decrypt_iv_;

    uint32_t session_type_ = 0; // Selected session type.
    QString user_name_;

private slots:
    void onTcpDisconnected(NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer);
    void onTcpMessageWritten(uint8_t channel_id, size_t pending);

private:
    QPointer<QTimer> timer_;
    QPointer<TcpChannel> channel_;
    State state_ = State::STOPPED;
    Version peer_version_; // Remote peer version.
    QString peer_os_name_;
    QString peer_computer_name_;
    QString peer_arch_;
    QString peer_display_name_;
};

} // namespace base

Q_DECLARE_METATYPE(base::Authenticator::ErrorCode)

#endif // BASE_PEER_AUTHENTICATOR_H
