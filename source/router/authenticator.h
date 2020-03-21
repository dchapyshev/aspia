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

#ifndef ROUTER__AUTHENTICATOR_H
#define ROUTER__AUTHENTICATOR_H

#include "base/waitable_timer.h"
#include "base/version.h"
#include "crypto/big_num.h"
#include "crypto/key_pair.h"
#include "net/network_listener.h"
#include "proto/key_exchange.pb.h"
#include "proto/router.pb.h"

namespace base {
class Location;
class TaskRunner;
} // namespace base

namespace net {
class Channel;
} // namespace net

namespace router {

class Session;
class UserList;

class Authenticator : public net::Listener
{
public:
    Authenticator(std::shared_ptr<base::TaskRunner> task_runner,
                  std::unique_ptr<net::Channel> channel);
    ~Authenticator();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onComplete() = 0;
    };

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };

    bool start(const base::ByteArray& private_key,
               std::shared_ptr<UserList> user_list,
               Delegate* delegate);

    State state() const { return state_; }
    proto::Identify identify() const { return identify_; }
    const std::u16string& userName() const { return username_; }
    const base::Version& peerVersion() const { return peer_version_; }

    std::unique_ptr<Session> takeSession();

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    void onSessionKeyChanged();
    void onFailed(const base::Location& location);
    void onClientHello(const base::ByteArray& buffer);
    void doServerHello();
    void onIdentify(const base::ByteArray& buffer);
    void doServerKeyExchange();
    void onClientKeyExchange(const base::ByteArray& buffer);
    void doSessionChallenge();
    void onSessionResponse(const base::ByteArray& buffer);

    base::ByteArray createSrpKey();

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

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<UserList> user_list_;

    base::WaitableTimer timer_;

    State state_ = State::STOPPED;
    Delegate* delegate_ = nullptr;

    InternalState internal_state_ = InternalState::READ_CLIENT_HELLO;

    proto::Encryption encryption_ = proto::ENCRYPTION_UNKNOWN;
    proto::Identify identify_ = proto::IDENTIFY_SRP;
    proto::RouterSession session_ = proto::ROUTER_SESSION_UNKNOWN;
    uint32_t session_types_ = 0;

    std::u16string username_;
    base::Version peer_version_;

    base::ByteArray session_key_;
    base::ByteArray encrypt_iv_;
    base::ByteArray decrypt_iv_;

    crypto::KeyPair key_pair_;
    crypto::BigNum N_;
    crypto::BigNum g_;
    crypto::BigNum v_;
    crypto::BigNum s_;
    crypto::BigNum b_;
    crypto::BigNum B_;
    crypto::BigNum A_;

    DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace router

#endif // ROUTER__AUTHENTICATOR_H
