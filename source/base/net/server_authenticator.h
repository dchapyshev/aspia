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

#ifndef BASE__NET__SERVER_AUTHENTICATOR_H
#define BASE__NET__SERVER_AUTHENTICATOR_H

#include "base/version.h"
#include "base/waitable_timer.h"
#include "base/crypto/big_num.h"
#include "base/crypto/key_pair.h"
#include "base/net/network_channel.h"
#include "proto/key_exchange.pb.h"

namespace base {

class Location;
class UserList;

class ServerAuthenticator : public NetworkChannel::Listener
{
public:
    explicit ServerAuthenticator(std::shared_ptr<TaskRunner> task_runner);
    ~ServerAuthenticator();

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };

    enum class AnonymousAccess
    {
        ENABLE, // Anonymous access is enabled.
        DISABLE // Anonymous access is disabled.
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onComplete() = 0;
    };

    // The start of the authenticator.
    void start(std::unique_ptr<NetworkChannel> channel,
               std::shared_ptr<UserList> user_list,
               Delegate* delegate);

    // Sets the private key.
    [[nodiscard]] bool setPrivateKey(const ByteArray& private_key);

    // Enables or disables anonymous access.
    // |session_types] allowed session types for anonymous access.
    // The private key must be set up for anonymous access.
    // By default, anonymous access is disabled.
    [[nodiscard]] bool setAnonymousAccess(AnonymousAccess anonymous_access, uint32_t session_types);

    // Returns the current state.
    [[nodiscard]] State state() const { return state_; }

    [[nodiscard]] uint32_t sessionType() const { return session_type_; }
    [[nodiscard]] const Version& peerVersion() const { return peer_version_; }
    [[nodiscard]] const std::u16string& userName() const { return user_name_; }

    [[nodiscard]] std::unique_ptr<NetworkChannel> takeChannel();

protected:
    // Channel::Listener implementation.
    void onConnected() override;
    void onDisconnected(NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void onClientHello(const ByteArray& buffer);
    void onIdentify(const ByteArray& buffer);
    void onClientKeyExchange(const ByteArray& buffer);
    void doSessionChallenge();
    void onSessionResponse(const ByteArray& buffer);
    void onFailed(const Location& location);
    [[nodiscard]] bool onSessionKeyChanged();
    [[nodiscard]] ByteArray createSrpKey();

    WaitableTimer timer_;
    std::unique_ptr<NetworkChannel> channel_;
    std::shared_ptr<UserList> user_list_;

    Delegate* delegate_ = nullptr;
    State state_ = State::STOPPED;

    enum class InternalState
    {
        READ_CLIENT_HELLO,
        SEND_SERVER_HELLO,
        READ_IDENTIFY,
        SEND_SERVER_KEY_EXCHANGE,
        READ_CLIENT_KEY_EXCHANGE,
        SEND_SESSION_CHALLENGE,
        READ_SESSION_RESPONSE
    };

    AnonymousAccess anonymous_access_ = AnonymousAccess::DISABLE;
    InternalState internal_state_ = InternalState::READ_CLIENT_HELLO;

    // Bitmask of allowed session types.
    uint32_t session_types_ = 0;

    // Selected session type.
    uint32_t session_type_ = 0;

    // Selected authentication method.
    proto::Encryption encryption_ = proto::ENCRYPTION_UNKNOWN;
    proto::Identify identify_ = proto::IDENTIFY_SRP;

    // Remote client version.
    Version peer_version_;

    // User name.
    std::u16string user_name_;

    ByteArray session_key_;
    ByteArray encrypt_iv_;
    ByteArray decrypt_iv_;

    KeyPair key_pair_;
    BigNum N_;
    BigNum g_;
    BigNum v_;
    BigNum s_;
    BigNum b_;
    BigNum B_;
    BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(ServerAuthenticator);
};

} // namespace base

#endif // BASE__NET__SERVER_AUTHENTICATOR_H
