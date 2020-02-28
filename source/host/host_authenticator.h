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

#ifndef HOST__HOST_AUTHENTICATOR_H
#define HOST__HOST_AUTHENTICATOR_H

#include "base/version.h"
#include "base/waitable_timer.h"
#include "crypto/big_num.h"
#include "net/network_listener.h"
#include "proto/key_exchange.pb.h"

namespace base {
class Location;
} // namespace base

namespace net {
class Channel;
} // namespace net

namespace host {

class ClientSession;
class UserList;

class Authenticator : public net::Listener
{
public:
    explicit Authenticator(std::shared_ptr<base::TaskRunner> task_runner);
    ~Authenticator();

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onComplete() = 0;
    };

    // The start of the authenticator.
    void start(std::unique_ptr<net::Channel> channel,
               std::shared_ptr<UserList> userlist,
               Delegate* delegate);

    // Returns the current state.
    State state() const { return state_; }

    // Prepares a session. It returns the correct value only in SUCCESS state.
    // Otherwise, it returns nullptr.
    std::unique_ptr<ClientSession> takeSession();

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    base::ByteArray createKey();
    void onFailed(const base::Location& location);

    base::WaitableTimer timer_;
    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<UserList> userlist_;

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

    InternalState internal_state_ = InternalState::READ_CLIENT_HELLO;

    // Bitmask of allowed session types for the user.
    uint32_t session_types_ = 0;

    // Selected session type.
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;

    // Selected authentication method.
    proto::Method method_ = proto::METHOD_UNKNOWN;

    // Remote client version.
    base::Version peer_version_;

    // User name.
    std::u16string username_;

    base::ByteArray encrypt_iv_;
    base::ByteArray decrypt_iv_;

    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum v_;
    crypto::BigNum s_;
    crypto::BigNum b_;
    crypto::BigNum B_;
    crypto::BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace host

#endif // HOST__HOST_AUTHENTICATOR_H
