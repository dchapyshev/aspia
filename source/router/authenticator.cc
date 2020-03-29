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

#include "router/authenticator.h"

#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/unicode.h"
#include "build/version.h"
#include "crypto/generic_hash.h"
#include "crypto/message_decryptor_openssl.h"
#include "crypto/message_encryptor_openssl.h"
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "router/session_manager.h"
#include "router/session_peer.h"
#include "router/user.h"

namespace router {

namespace {

constexpr std::chrono::minutes kTimeout{ 1 };

} // namespace

Authenticator::Authenticator(std::shared_ptr<base::TaskRunner> task_runner,
                             std::unique_ptr<net::Channel> channel)
    : task_runner_(task_runner),
      channel_(std::move(channel)),
      timer_(task_runner)
{
    DCHECK(task_runner_ && channel_);
}

Authenticator::~Authenticator() = default;

bool Authenticator::start(
    const base::ByteArray& private_key, std::shared_ptr<UserList> user_list, Delegate* delegate)
{
    DCHECK_EQ(internal_state_, InternalState::READ_CLIENT_HELLO);

    key_pair_ = crypto::KeyPair::fromPrivateKey(private_key);
    if (!key_pair_.isValid())
        return false;

    user_list_ = std::move(user_list);
    if (!user_list)
        return false;

    delegate_ = delegate;
    if (!delegate_)
        return false;

    state_ = State::PENDING;

    timer_.start(kTimeout, std::bind(&Authenticator::onFailed, this, FROM_HERE));

    channel_->setListener(this);
    channel_->resume();

    return true;
}

std::unique_ptr<Session> Authenticator::takeSession()
{
    if (state_ != State::SUCCESS)
        return nullptr;

    std::unique_ptr<Session> session;

    switch (session_)
    {
        case proto::ROUTER_SESSION_PEER:
            session = std::make_unique<SessionPeer>(std::move(channel_));
            break;

        case proto::ROUTER_SESSION_MANAGER:
            session = std::make_unique<SessionManager>(std::move(channel_));
            break;

        default:
            break;
    }

    if (session)
    {
        session->setVersion(peer_version_);
        session->setUserName(username_);
    }

    return session;
}

void Authenticator::onConnected()
{
    NOTREACHED();
}

void Authenticator::onDisconnected(net::Channel::ErrorCode /* error_code */)
{
    onFailed(FROM_HERE);
}

void Authenticator::onMessageReceived(const base::ByteArray& buffer)
{
    switch (internal_state_)
    {
        case InternalState::READ_CLIENT_HELLO:
            onClientHello(buffer);
            break;

        case InternalState::READ_IDENTIFY:
            onIdentify(buffer);
            break;

        case InternalState::READ_CLIENT_KEY_EXCHANGE:
            onClientKeyExchange(buffer);
            break;

        case InternalState::READ_SESSION_RESPONSE:
            onSessionResponse(buffer);
            break;

        default:
            NOTREACHED();
            break;
    }
}

void Authenticator::onMessageWritten()
{
    switch (internal_state_)
    {
        case InternalState::SEND_SERVER_HELLO:
        {
            onSessionKeyChanged();

            if (identify_ == proto::IDENTIFY_ANONYMOUS)
            {
                doSessionChallenge();
            }
            else
            {
                DCHECK_EQ(identify_, proto::IDENTIFY_SRP);
                internal_state_ = InternalState::READ_IDENTIFY;
            }
        }
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

void Authenticator::onSessionKeyChanged()
{
    std::unique_ptr<crypto::MessageEncryptor> encryptor;
    std::unique_ptr<crypto::MessageDecryptor> decryptor;

    if (encryption_ == proto::ENCRYPTION_AES256_GCM)
    {
        encryptor = crypto::MessageEncryptorOpenssl::createForAes256Gcm(
            session_key_, encrypt_iv_);
        decryptor = crypto::MessageDecryptorOpenssl::createForAes256Gcm(
            session_key_, decrypt_iv_);
    }
    else
    {
        DCHECK_EQ(encryption_, proto::ENCRYPTION_CHACHA20_POLY1305);

        encryptor = crypto::MessageEncryptorOpenssl::createForChaCha20Poly1305(
            session_key_, encrypt_iv_);
        decryptor = crypto::MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key_, decrypt_iv_);
    }

    if (!encryptor || !decryptor)
    {
        onFailed(FROM_HERE);
        return;
    }

    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));
}

void Authenticator::onFailed(const base::Location& location)
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

void Authenticator::onClientHello(const base::ByteArray& buffer)
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

    if ((client_hello.encryption() & proto::ENCRYPTION_AES256_GCM) && base::CPUID::hasAesNi())
    {
        // If both sides of the connection support AES, then method AES256 GCM is the
        // fastest option.
        encryption_ = proto::ENCRYPTION_AES256_GCM;
    }
    else
    {
        // Otherwise, we use ChaCha20+Poly1305. This works faster in the absence of
        // hardware support AES.
        encryption_ = proto::ENCRYPTION_CHACHA20_POLY1305;
    }

    identify_ = client_hello.identify();

    if (identify_ != proto::IDENTIFY_ANONYMOUS && identify_ != proto::IDENTIFY_SRP)
    {
        // No authentication methods supported.
        onFailed(FROM_HERE);
        return;
    }

    base::ByteArray temp = key_pair_.sessionKey(base::fromStdString(client_hello.public_key()));
    if (temp.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    session_key_ = crypto::GenericHash::hash(crypto::GenericHash::BLAKE2s256, temp);
    if (session_key_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    decrypt_iv_ = base::fromStdString(client_hello.iv());
    if (decrypt_iv_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    encrypt_iv_ = crypto::Random::byteArray(decrypt_iv_.size());
    if (encrypt_iv_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    doServerHello();
}

void Authenticator::doServerHello()
{
    internal_state_ = InternalState::SEND_SERVER_HELLO;

    proto::ServerHello server_hello;
    server_hello.set_encryption(encryption_);
    server_hello.set_iv(base::toStdString(encrypt_iv_));

    channel_->send(base::serialize(server_hello));
}

void Authenticator::onIdentify(const base::ByteArray& buffer)
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

    const User& user = user_list_->find(username_);
    if (!user.isValid())
    {
        session_types_ = proto::ROUTER_SESSION_PEER;

        crypto::GenericHash hash(crypto::GenericHash::BLAKE2b512);
        hash.addData(user_list_->seedKey());
        hash.addData(identify.username());

        N_ = crypto::BigNum::fromStdString(crypto::kSrpNgPair_8192.first);
        g_ = crypto::BigNum::fromStdString(crypto::kSrpNgPair_8192.second);
        s_ = crypto::BigNum::fromByteArray(hash.result());
        v_ = crypto::SrpMath::calc_v(username_, user_list_->seedKey(), s_, N_, g_);
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

    encrypt_iv_ = crypto::Random::byteArray(12);
    if (encrypt_iv_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    doServerKeyExchange();
}

void Authenticator::doServerKeyExchange()
{
    internal_state_ = InternalState::SEND_SERVER_KEY_EXCHANGE;

    proto::SrpServerKeyExchange server_key_exchange;
    server_key_exchange.set_number(N_.toStdString());
    server_key_exchange.set_generator(g_.toStdString());
    server_key_exchange.set_salt(s_.toStdString());
    server_key_exchange.set_b(B_.toStdString());
    server_key_exchange.set_iv(base::toStdString(encrypt_iv_));

    channel_->send(base::serialize(server_key_exchange));
}

void Authenticator::onClientKeyExchange(const base::ByteArray& buffer)
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

    base::ByteArray srp_key = createSrpKey();
    if (srp_key.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    // After SRP key exchange, we generate a new session key.
    crypto::GenericHash hash(crypto::GenericHash::BLAKE2s256);

    // The new session key consists of the hash of the old key and the hash of the SRP key.
    hash.addData(session_key_);
    hash.addData(srp_key);

    session_key_ = hash.result();
    if (session_key_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    onSessionKeyChanged();
    doSessionChallenge();
}

void Authenticator::doSessionChallenge()
{
    internal_state_ = InternalState::SEND_SESSION_CHALLENGE;

    proto::SessionChallenge session_challenge;
    session_challenge.set_session_types(session_types_);

    proto::Version* version = session_challenge.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);

    channel_->send(base::serialize(session_challenge));
}

void Authenticator::onSessionResponse(const base::ByteArray& buffer)
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

    session_ = static_cast<proto::RouterSession>(session_response.session_type());

    timer_.stop();

    // Authentication completed successfully.
    state_ = State::SUCCESS;

    // Notify of completion.
    delegate_->onComplete();
}

base::ByteArray Authenticator::createSrpKey()
{
    if (!crypto::SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_ERROR) << "SrpMath::verify_A_mod_N failed";
        return base::ByteArray();
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum server_key = crypto::SrpMath::calcServerKey(A_, v_, u, b_, N_);

    return server_key.toByteArray();
}

} // namespace router
