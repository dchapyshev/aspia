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

#include "host/host_authenticator.h"

#include "base/cpuid.h"
#include "base/version.h"
#include "build/version.h"
#include "common/message_serialization.h"
#include "crypto/cryptor_aes256_gcm.h"
#include "crypto/cryptor_chacha20_poly1305.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "host/client_session.h"
#include "host/user.h"
#include "net/network_channel.h"
#include "proto/key_exchange.pb.h"
#include "qt_base/qt_logging.h"

namespace host {

Authenticator::Authenticator() = default;
Authenticator::~Authenticator() = default;

void Authenticator::start(std::unique_ptr<net::Channel> channel,
                          std::shared_ptr<UserList> userlist,
                          Delegate* delegate)
{
    channel_ = std::move(channel);
    userlist_ = userlist;
    delegate_ = delegate;

    DCHECK_EQ(internal_state_, InternalState::HELLO);
    DCHECK(channel_);
    DCHECK(userlist_);
    DCHECK(delegate_);

    state_ = State::PENDING;

    channel_->setListener(this);
    channel_->resume();

    // We are waiting for message ClientHello from the client.
    LOG(LS_INFO) << "Authentication started for " << channel_->peerAddress();
}

std::unique_ptr<ClientSession> Authenticator::takeSession()
{
    if (state_ != State::SUCCESS)
        return nullptr;

    std::unique_ptr<ClientSession> session =
        ClientSession::create(session_type_, std::move(channel_));

    if (session)
    {
        session->setVersion(peer_version_);
        session->setUserName(username_);
    }

    return session;
}

void Authenticator::onNetworkConnected()
{
    NOTREACHED();
}

void Authenticator::onNetworkDisconnected()
{
    onFailed();
}

void Authenticator::onNetworkError(net::ErrorCode error_code)
{
    // We only write errors to the log.
    // If an error occurs, the connection will also be disconnected; error handling is there.
    LOG(LS_WARNING) << "Network error: " << static_cast<int>(error_code);
}

void Authenticator::onNetworkMessage(const base::ByteArray& buffer)
{
    switch (internal_state_)
    {
        case InternalState::HELLO:
        {
            proto::ClientHello client_hello;

            if (!common::parseMessage(buffer, &client_hello))
            {
                onFailed();
                return;
            }

            if (!(client_hello.methods() & proto::METHOD_SRP_AES256_GCM) &&
                !(client_hello.methods() & proto::METHOD_SRP_CHACHA20_POLY1305))
            {
                // No authentication methods supported.
                onFailed();
                return;
            }

            proto::ServerHello server_hello;

            if ((client_hello.methods() & proto::METHOD_SRP_AES256_GCM) && base::CPUID::hasAesNi())
            {
                // If both sides of the connection support AES, then method AES256 GCM is the
                // fastest option.
                server_hello.set_method(proto::METHOD_SRP_AES256_GCM);
            }
            else
            {
                // Otherwise, we use ChaCha20+Poly1305. This works faster in the absence of
                // hardware support AES.
                server_hello.set_method(proto::METHOD_SRP_CHACHA20_POLY1305);
            }

            // Now we are in the authentication phase.
            internal_state_ = InternalState::IDENTIFY;
            method_ = server_hello.method();

            channel_->send(common::serializeMessage(server_hello));
        }
        break;

        case InternalState::IDENTIFY:
        {
            proto::SrpIdentify identify;

            if (!common::parseMessage(buffer, &identify))
            {
                onFailed();
                return;
            }

            username_ = QString::fromStdString(identify.username());
            if (username_.isEmpty())
            {
                onFailed();
                return;
            }

            const int user_index = userlist_->find(username_);
            if (user_index == -1)
            {
                session_types_ = proto::SESSION_TYPE_ALL;

                crypto::GenericHash hash(crypto::GenericHash::BLAKE2b512);
                hash.addData(userlist_->seedKey());
                hash.addData(identify.username());

                N_ = crypto::BigNum::fromBuffer(crypto::kSrpNg_8192.N);
                g_ = crypto::BigNum::fromBuffer(crypto::kSrpNg_8192.g);
                s_ = crypto::BigNum::fromByteArray(hash.result());
                v_ = crypto::SrpMath::calc_v(username_, userlist_->seedKey(), s_, N_, g_);
            }
            else
            {
                const User& user = userlist_->at(user_index);

                session_types_ = user.sessions;

                N_ = crypto::BigNum::fromStdString(user.number);
                g_ = crypto::BigNum::fromStdString(user.generator);
                s_ = crypto::BigNum::fromStdString(user.salt);
                v_ = crypto::BigNum::fromStdString(user.verifier);
            }

            b_ = crypto::BigNum::fromByteArray(crypto::Random::byteArray(128)); // 1024 bits.
            B_ = crypto::SrpMath::calc_B(b_, N_, g_, v_);

            if (!N_.isValid() || !g_.isValid() || !s_.isValid() || !B_.isValid())
            {
                onFailed();
                return;
            }

            internal_state_ = InternalState::KEY_EXCHANGE;
            encrypt_iv_ = crypto::Random::byteArray(12);

            proto::SrpServerKeyExchange server_key_exchange;

            server_key_exchange.set_number(N_.toStdString());
            server_key_exchange.set_generator(g_.toStdString());
            server_key_exchange.set_salt(s_.toStdString());
            server_key_exchange.set_b(B_.toStdString());
            server_key_exchange.set_iv(base::toStdString(encrypt_iv_));

            channel_->send(common::serializeMessage(server_key_exchange));
        }
        break;

        case InternalState::KEY_EXCHANGE:
        {
            proto::SrpClientKeyExchange client_key_exchange;

            if (!common::parseMessage(buffer, &client_key_exchange))
            {
                onFailed();
                return;
            }

            A_ = crypto::BigNum::fromStdString(client_key_exchange.a());
            decrypt_iv_ = base::fromStdString(client_key_exchange.iv());

            if (!A_.isValid() || decrypt_iv_.empty())
            {
                onFailed();
                return;
            }

            std::unique_ptr<crypto::Cryptor> cryptor = takeCryptor();
            if (!cryptor)
            {
                onFailed();
                return;
            }

            internal_state_ = InternalState::SESSION;
            channel_->setCryptor(std::move(cryptor));

            proto::SessionChallenge session_challenge;
            session_challenge.set_session_types(session_types_);

            proto::Version* version = session_challenge.mutable_version();
            version->set_major(ASPIA_VERSION_MAJOR);
            version->set_minor(ASPIA_VERSION_MINOR);
            version->set_patch(ASPIA_VERSION_PATCH);

            channel_->send(common::serializeMessage(session_challenge));
        }
        break;

        case InternalState::SESSION:
        {
            // Stop receiving incoming messages.
            channel_->setListener(nullptr);
            channel_->pause();

            proto::SessionResponse session_response;
            if (!common::parseMessage(buffer, &session_response))
            {
                onFailed();
                return;
            }

            const proto::Version& peer_version = session_response.version();
            peer_version_ = base::Version(
                peer_version.major(), peer_version.minor(), peer_version.patch());

            session_type_ = session_response.session_type();

            LOG(LS_INFO) << "Authentication completed successfully for "
                         << channel_->peerAddress();

            // Authentication completed successfully.
            state_ = State::SUCCESS;

            // Notify of completion.
            delegate_->onComplete();
        }
        break;

        default:
            NOTREACHED();
            break;
    }
}

std::unique_ptr<crypto::Cryptor> Authenticator::takeCryptor()
{
    if (!crypto::SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_ERROR) << "SrpMath::verify_A_mod_N failed";
        return nullptr;
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum server_key = crypto::SrpMath::calcServerKey(A_, v_, u, b_, N_);
    std::unique_ptr<crypto::Cryptor> cryptor;

    switch (method_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
        {
            base::ByteArray temp = server_key.toByteArray();
            if (!temp.empty())
            {
                base::ByteArray key =
                    crypto::GenericHash::hash(crypto::GenericHash::BLAKE2s256, temp);

                crypto::memZero(&temp);

                if (method_ == proto::METHOD_SRP_AES256_GCM)
                {
                    cryptor = crypto::CryptorAes256Gcm::create(
                        std::move(key), std::move(encrypt_iv_), std::move(decrypt_iv_));
                }
                else
                {
                    DCHECK_EQ(method_, proto::METHOD_SRP_CHACHA20_POLY1305);

                    cryptor = crypto::CryptorChaCha20Poly1305::create(
                        std::move(key), std::move(encrypt_iv_), std::move(decrypt_iv_));
                }
            }
        }
        break;

        default:
        {
            LOG(LS_ERROR) << "Unknown method: " << method_;
        }
        break;
    }

    return cryptor;
}

void Authenticator::onFailed()
{
    // If the network channel is already destroyed, then exit (we have a repeated notification).
    if (!channel_)
        return;

    LOG(LS_INFO) << "Authentication failed for " << channel_->peerAddress();

    // Destroy the network channel.
    channel_->setListener(nullptr);
    channel_.reset();

    // A connection failure occurred, authentication failed.
    state_ = State::FAILED;

    // Notify of completion.
    delegate_->onComplete();
}

} // namespace host
