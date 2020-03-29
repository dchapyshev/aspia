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

#include "net/server_authenticator.h"

#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/unicode.h"
#include "build/version.h"
#include "crypto/message_decryptor_openssl.h"
#include "crypto/message_encryptor_openssl.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "net/network_channel.h"
#include "net/server_user.h"

namespace net {

namespace {

constexpr std::chrono::minutes kTimeout{ 1 };

} // namespace

ServerAuthenticator::ServerAuthenticator(std::shared_ptr<base::TaskRunner> task_runner)
    : timer_(std::move(task_runner))
{
    // Nothing
}

ServerAuthenticator::~ServerAuthenticator() = default;

void ServerAuthenticator::start(std::unique_ptr<Channel> channel,
                                std::shared_ptr<ServerUserList> userlist,
                                Delegate* delegate)
{
    channel_ = std::move(channel);
    userlist_ = std::move(userlist);
    delegate_ = delegate;

    DCHECK_EQ(internal_state_, InternalState::READ_CLIENT_HELLO);
    DCHECK(channel_);
    DCHECK(userlist_);
    DCHECK(delegate_);

    state_ = State::PENDING;

    timer_.start(kTimeout, std::bind(&ServerAuthenticator::onFailed, this, FROM_HERE));

    channel_->setListener(this);
    channel_->resume();

    // We are waiting for message ClientHello from the client.
    LOG(LS_INFO) << "Authentication started for: " << channel_->peerAddress();
}

std::unique_ptr<Channel> ServerAuthenticator::takeChannel()
{
    if (state_ != State::SUCCESS)
        return nullptr;

    return std::move(channel_);
}

void ServerAuthenticator::onConnected()
{
    NOTREACHED();
}

void ServerAuthenticator::onDisconnected(net::ErrorCode error_code)
{
    LOG(LS_WARNING) << "Network error: " << net::errorToString(error_code);
    onFailed(FROM_HERE);
}

void ServerAuthenticator::onMessageReceived(const base::ByteArray& buffer)
{
    switch (internal_state_)
    {
        case InternalState::READ_CLIENT_HELLO:
        {
            proto::ClientHello client_hello;

            if (!base::parse(buffer, &client_hello))
            {
                onFailed(FROM_HERE);
                return;
            }

            if (!(client_hello.encryption() & proto::ENCRYPTION_AES256_GCM) &&
                !(client_hello.encryption() & proto::ENCRYPTION_CHACHA20_POLY1305))
            {
                // No encryption methods supported.
                onFailed(FROM_HERE);
                return;
            }

            proto::ServerHello server_hello;

            if ((client_hello.encryption() & proto::ENCRYPTION_AES256_GCM) &&
                base::CPUID::hasAesNi())
            {
                // If both sides of the connection support AES, then method AES256 GCM is the
                // fastest option.
                server_hello.set_encryption(proto::ENCRYPTION_AES256_GCM);
            }
            else
            {
                // Otherwise, we use ChaCha20+Poly1305. This works faster in the absence of
                // hardware support AES.
                server_hello.set_encryption(proto::ENCRYPTION_CHACHA20_POLY1305);
            }

            // Now we are in the authentication phase.
            internal_state_ = InternalState::SEND_SERVER_HELLO;
            encryption_ = server_hello.encryption();

            channel_->send(base::serialize(server_hello));
        }
        break;

        case InternalState::READ_IDENTIFY:
        {
            proto::SrpIdentify identify;

            if (!base::parse(buffer, &identify))
            {
                onFailed(FROM_HERE);
                return;
            }

            username_ = base::utf16FromUtf8(identify.username());
            if (username_.empty())
            {
                onFailed(FROM_HERE);
                return;
            }

            const ServerUser& user = userlist_->find(username_);
            if (!user.isValid())
            {
                session_types_ = proto::SESSION_TYPE_ALL;

                crypto::GenericHash hash(crypto::GenericHash::BLAKE2b512);
                hash.addData(userlist_->seedKey());
                hash.addData(identify.username());

                N_ = crypto::BigNum::fromStdString(crypto::kSrpNgPair_8192.first);
                g_ = crypto::BigNum::fromStdString(crypto::kSrpNgPair_8192.second);
                s_ = crypto::BigNum::fromByteArray(hash.result());
                v_ = crypto::SrpMath::calc_v(username_, userlist_->seedKey(), s_, N_, g_);
            }
            else
            {
                session_types_ = user.sessions;

                N_ = crypto::BigNum::fromByteArray(user.number);
                g_ = crypto::BigNum::fromByteArray(user.generator);
                s_ = crypto::BigNum::fromByteArray(user.salt);
                v_ = crypto::BigNum::fromByteArray(user.verifier);
            }

            b_ = crypto::BigNum::fromByteArray(crypto::Random::byteArray(128)); // 1024 bits.
            B_ = crypto::SrpMath::calc_B(b_, N_, g_, v_);

            if (!N_.isValid() || !g_.isValid() || !s_.isValid() || !B_.isValid())
            {
                onFailed(FROM_HERE);
                return;
            }

            internal_state_ = InternalState::SEND_SERVER_KEY_EXCHANGE;
            encrypt_iv_ = crypto::Random::byteArray(12);

            proto::SrpServerKeyExchange server_key_exchange;

            server_key_exchange.set_number(N_.toStdString());
            server_key_exchange.set_generator(g_.toStdString());
            server_key_exchange.set_salt(s_.toStdString());
            server_key_exchange.set_b(B_.toStdString());
            server_key_exchange.set_iv(base::toStdString(encrypt_iv_));

            channel_->send(base::serialize(server_key_exchange));
        }
        break;

        case InternalState::READ_CLIENT_KEY_EXCHANGE:
        {
            proto::SrpClientKeyExchange client_key_exchange;

            if (!base::parse(buffer, &client_key_exchange))
            {
                onFailed(FROM_HERE);
                return;
            }

            A_ = crypto::BigNum::fromStdString(client_key_exchange.a());
            decrypt_iv_ = base::fromStdString(client_key_exchange.iv());

            if (!A_.isValid() || decrypt_iv_.empty())
            {
                onFailed(FROM_HERE);
                return;
            }

            base::ByteArray key = createKey();

            std::unique_ptr<crypto::MessageEncryptor> encryptor;
            std::unique_ptr<crypto::MessageDecryptor> decryptor;

            if (encryption_ == proto::ENCRYPTION_AES256_GCM)
            {
                encryptor = crypto::MessageEncryptorOpenssl::createForAes256Gcm(key, encrypt_iv_);
                decryptor = crypto::MessageDecryptorOpenssl::createForAes256Gcm(key, decrypt_iv_);
            }
            else
            {
                DCHECK_EQ(encryption_, proto::ENCRYPTION_CHACHA20_POLY1305);

                encryptor =
                    crypto::MessageEncryptorOpenssl::createForChaCha20Poly1305(key, encrypt_iv_);
                decryptor =
                    crypto::MessageDecryptorOpenssl::createForChaCha20Poly1305(key, decrypt_iv_);
            }

            crypto::memZero(&key);

            if (!encryptor || !decryptor)
            {
                onFailed(FROM_HERE);
                return;
            }

            internal_state_ = InternalState::SEND_SESSION_CHALLENGE;

            channel_->setEncryptor(std::move(encryptor));
            channel_->setDecryptor(std::move(decryptor));

            proto::SessionChallenge session_challenge;
            session_challenge.set_session_types(session_types_);

            proto::Version* version = session_challenge.mutable_version();
            version->set_major(ASPIA_VERSION_MAJOR);
            version->set_minor(ASPIA_VERSION_MINOR);
            version->set_patch(ASPIA_VERSION_PATCH);

            channel_->send(base::serialize(session_challenge));
        }
        break;

        case InternalState::READ_SESSION_RESPONSE:
        {
            // Stop receiving incoming messages.
            channel_->setListener(nullptr);
            channel_->pause();

            proto::SessionResponse session_response;
            if (!base::parse(buffer, &session_response))
            {
                onFailed(FROM_HERE);
                return;
            }

            const proto::Version& peer_version = session_response.version();
            peer_version_ = base::Version(
                peer_version.major(), peer_version.minor(), peer_version.patch());

            session_type_ = static_cast<proto::SessionType>(session_response.session_type());

            LOG(LS_INFO) << "Authentication completed successfully for "
                         << channel_->peerAddress();

            timer_.stop();

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

void ServerAuthenticator::onMessageWritten()
{
    switch (internal_state_)
    {
        case InternalState::SEND_SERVER_HELLO:
            internal_state_ = InternalState::READ_IDENTIFY;
            break;

        case InternalState::SEND_SERVER_KEY_EXCHANGE:
            internal_state_ = InternalState::READ_CLIENT_KEY_EXCHANGE;
            break;

        case InternalState::SEND_SESSION_CHALLENGE:
            internal_state_ = InternalState::READ_SESSION_RESPONSE;
            break;

        default:
            NOTREACHED();
            break;
    }
}

base::ByteArray ServerAuthenticator::createKey()
{
    if (!crypto::SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_ERROR) << "SrpMath::verify_A_mod_N failed";
        return base::ByteArray();
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum server_key = crypto::SrpMath::calcServerKey(A_, v_, u, b_, N_);

    switch (encryption_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::ENCRYPTION_AES256_GCM:
        case proto::ENCRYPTION_CHACHA20_POLY1305:
        {
            base::ByteArray temp = server_key.toByteArray();
            if (!temp.empty())
            {
                base::ByteArray key = crypto::GenericHash::hash(
                    crypto::GenericHash::BLAKE2s256, temp);

                crypto::memZero(&temp);
                return key;
            }
        }
        break;

        default:
        {
            LOG(LS_ERROR) << "Unknown encryption method: " << encryption_;
        }
        break;
    }

    return base::ByteArray();
}

void ServerAuthenticator::onFailed(const base::Location& location)
{
    // If the network channel is already destroyed, then exit (we have a repeated notification).
    if (!channel_)
        return;

    LOG(LS_INFO) << "Authentication failed for: " << channel_->peerAddress()
                 << " (" << location.toString() << ")";

    timer_.stop();

    // Destroy the network channel.
    channel_->setListener(nullptr);
    channel_.reset();

    // A connection failure occurred, authentication failed.
    state_ = State::FAILED;

    // Notify of completion.
    delegate_->onComplete();
}

} // namespace net
