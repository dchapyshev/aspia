//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef PEER__AUTHENTICATOR_H
#define PEER__AUTHENTICATOR_H

#include "base/waitable_timer.h"
#include "base/version.h"
#include "base/net/network_channel.h"
#include "proto/key_exchange.pb.h"

namespace base {
class Location;
} // namespace base

namespace peer {

class Authenticator : public base::NetworkChannel::Listener
{
public:
    explicit Authenticator(std::shared_ptr<base::TaskRunner> task_runner);
    virtual ~Authenticator() = default;

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
        ACCESS_DENIED,
        SESSION_DENIED
    };

    using Callback = std::function<void(ErrorCode error_code)>;

    void start(std::unique_ptr<base::NetworkChannel> channel, Callback callback);

    [[nodiscard]] proto::Identify identify() const { return identify_; }
    [[nodiscard]] proto::Encryption encryption() const { return encryption_; }
    [[nodiscard]] const base::Version& peerVersion() const { return peer_version_; }
    [[nodiscard]] uint32_t sessionType() const { return session_type_; }
    [[nodiscard]] const std::u16string& userName() const { return user_name_; }

    // Returns the current state.
    [[nodiscard]] State state() const { return state_; }

    // Releases network channel.
    [[nodiscard]] std::unique_ptr<base::NetworkChannel> takeChannel();

    static const char* osTypeToString(proto::OsType os_type);
    static const char* stateToString(State state);
    static const char* errorToString(Authenticator::ErrorCode error_code);

protected:
    [[nodiscard]] virtual bool onStarted() = 0;
    virtual void onReceived(const base::ByteArray& buffer) = 0;
    virtual void onWritten() = 0;

    void sendMessage(const google::protobuf::MessageLite& message);
    void finish(const base::Location& location, ErrorCode error_code);
    void setPeerVersion(const proto::Version& version);

    // base::NetworkChannel::Listener implementation.
    void onConnected() final;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) final;
    void onMessageReceived(const base::ByteArray& buffer) final;
    void onMessageWritten(size_t pending) final;

    [[nodiscard]] bool onSessionKeyChanged();

    proto::Encryption encryption_ = proto::ENCRYPTION_UNKNOWN;
    proto::Identify identify_ = proto::IDENTIFY_SRP;
    base::ByteArray session_key_;
    base::ByteArray encrypt_iv_;
    base::ByteArray decrypt_iv_;

    uint32_t session_type_ = 0; // Selected session type.
    std::u16string user_name_;

private:
    base::WaitableTimer timer_;
    std::unique_ptr<base::NetworkChannel> channel_;
    Callback callback_;
    State state_ = State::STOPPED;

    base::Version peer_version_; // Remote peer version.
};

} // namespace peer

#endif // PEER__AUTHENTICATOR_H
