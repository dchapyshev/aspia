//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "crypto/big_num.h"
#include "net/network_listener.h"
#include "proto/key_exchange.pb.h"

#include <QString>

namespace crypto {
class Cryptor;
} // namespace crypto

namespace net {
class Channel;
} // namespace net

namespace host {

class ClientSession;
class UserList;

class Authenticator : public net::Listener
{
public:
    Authenticator();
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
    void onNetworkConnected() override;
    void onNetworkDisconnected() override;
    void onNetworkError(net::ErrorCode error_code) override;
    void onNetworkMessage(const base::ByteArray& buffer) override;

private:
    std::unique_ptr<crypto::Cryptor> takeCryptor();
    void onFailed();

    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<UserList> userlist_;

    Delegate* delegate_ = nullptr;
    State state_ = State::STOPPED;

    enum class InternalState
    {
        HELLO,          // Detection state of supported authentication methods.
        IDENTIFY,       // Username definition.
        KEY_EXCHANGE,   // Key exchange.
        SESSION         // Authentication is completed, a session request occurs.
    };

    InternalState internal_state_ = InternalState::HELLO;

    // Bitmask of allowed session types for the user.
    uint32_t session_types_ = 0;

    // Selected session type.
    proto::SessionType session_type_ = proto::SESSION_TYPE_UNKNOWN;

    // Selected authentication method.
    proto::Method method_ = proto::METHOD_UNKNOWN;

    // Remote client version.
    base::Version peer_version_;

    // User name.
    QString username_;

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
