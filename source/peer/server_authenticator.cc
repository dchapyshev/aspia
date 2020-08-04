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

#include "peer/server_authenticator.h"

#include "base/bitset.h"
#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "base/crypto/message_encryptor_openssl.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/srp_constants.h"
#include "base/crypto/srp_math.h"
#include "base/strings/unicode.h"
#include "build/version.h"
#include "peer/user.h"

namespace peer {

namespace {

constexpr std::chrono::minutes kTimeout{ 1 };
constexpr size_t kIvSize = 12;

} // namespace

ServerAuthenticator::ServerAuthenticator(std::shared_ptr<base::TaskRunner> task_runner)
    : timer_(std::move(task_runner))
{
    // Nothing
}

ServerAuthenticator::~ServerAuthenticator() = default;

void ServerAuthenticator::start(std::unique_ptr<base::NetworkChannel> channel,
                                std::shared_ptr<UserList> user_list,
                                Delegate* delegate)
{
    if (state_ != State::STOPPED)
    {
        LOG(LS_ERROR) << "Trying to start an already running authenticator";
        return;
    }

    channel_ = std::move(channel);
    user_list_ = std::move(user_list);
    delegate_ = delegate;

    DCHECK_EQ(internal_state_, InternalState::READ_CLIENT_HELLO);
    DCHECK(channel_);
    DCHECK(user_list_);
    DCHECK(delegate_);

    state_ = State::PENDING;

    // We do not allow anonymous access without a private key.
    if (anonymous_access_ == AnonymousAccess::ENABLE && !key_pair_.isValid())
    {
        onFailed(FROM_HERE);
        return;
    }

    if (anonymous_access_ == AnonymousAccess::ENABLE)
    {
        // When anonymous access is enabled, a private key must be installed.
        if (!key_pair_.isValid())
        {
            onFailed(FROM_HERE);
            return;
        }

        // When anonymous access is enabled, there must be at least one session for anonymous access.
        if (!session_types_)
        {
            onFailed(FROM_HERE);
            return;
        }
    }
    else
    {
        // If anonymous access is disabled, then there should not be allowed sessions by default.
        if (session_types_)
        {
            onFailed(FROM_HERE);
            return;
        }
    }

    // If authentication does not complete within the specified time interval, an error will be
    // raised.
    timer_.start(kTimeout, std::bind(&ServerAuthenticator::onFailed, this, FROM_HERE));

    channel_->setListener(this);
    channel_->resume();

    // We are waiting for message ClientHello from the client.
    LOG(LS_INFO) << "Authentication started for: " << channel_->peerAddress();
}

bool ServerAuthenticator::setPrivateKey(const base::ByteArray& private_key)
{
    // The method must be called before calling start().
    if (state_ != State::STOPPED)
        return false;

    if (private_key.empty())
    {
        LOG(LS_ERROR) << "An empty private key is not valid";
        return false;
    }

    key_pair_ = base::KeyPair::fromPrivateKey(private_key);
    if (!key_pair_.isValid())
    {
        LOG(LS_ERROR) << "Failed to load private key. Perhaps the key is incorrect";
        return false;
    }

    encrypt_iv_ = base::Random::byteArray(kIvSize);
    if (encrypt_iv_.empty())
    {
        LOG(LS_ERROR) << "An empty IV is not valid";
        return false;
    }

    return true;
}

bool ServerAuthenticator::setAnonymousAccess(
    AnonymousAccess anonymous_access, uint32_t session_types)
{
    // The method must be called before calling start().
    if (state_ != State::STOPPED)
        return false;

    if (anonymous_access == AnonymousAccess::ENABLE)
    {
        if (!key_pair_.isValid())
        {
            LOG(LS_ERROR) << "When anonymous access is enabled, a private key must be installed";
            return false;
        }

        if (!session_types)
        {
            LOG(LS_ERROR) << "When anonymous access is enabled, there must be at least one "
                          << "session for anonymous access";
            return false;
        }

        session_types_ = session_types;
    }
    else
    {
        session_types_ = 0;
    }

    anonymous_access_ = anonymous_access;
    return true;
}

std::unique_ptr<base::NetworkChannel> ServerAuthenticator::takeChannel()
{
    if (state_ != State::SUCCESS)
        return nullptr;

    return std::move(channel_);
}

void ServerAuthenticator::onConnected()
{
    NOTREACHED();
}

void ServerAuthenticator::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_WARNING) << "Network error: " << base::NetworkChannel::errorToString(error_code);
    onFailed(FROM_HERE);
}

void ServerAuthenticator::onMessageReceived(const base::ByteArray& buffer)
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

void ServerAuthenticator::onMessageWritten(size_t /* pending */)
{
    switch (internal_state_)
    {
        case InternalState::SEND_SERVER_HELLO:
        {
            LOG(LS_INFO) << "Sended: ServerHello";

            if (!session_key_.empty())
            {
                if (!onSessionKeyChanged())
                    return;
            }

            switch (identify_)
            {
                case proto::IDENTIFY_SRP:
                {
                    internal_state_ = InternalState::READ_IDENTIFY;
                }
                break;

                case proto::IDENTIFY_ANONYMOUS:
                {
                    internal_state_ = InternalState::SEND_SESSION_CHALLENGE;
                    doSessionChallenge();
                }
                break;

                default:
                    NOTREACHED();
                    break;
            }
        }
        break;

        case InternalState::SEND_SERVER_KEY_EXCHANGE:
        {
            LOG(LS_INFO) << "Sended: ServerKeyExchange";
            internal_state_ = InternalState::READ_CLIENT_KEY_EXCHANGE;
        }
        break;

        case InternalState::SEND_SESSION_CHALLENGE:
        {
            LOG(LS_INFO) << "Sended: SessionChallenge";
            internal_state_ = InternalState::READ_SESSION_RESPONSE;
        }
        break;

        default:
            NOTREACHED();
            break;
    }
}

void ServerAuthenticator::onClientHello(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: ClientHello";

    proto::ClientHello client_hello;
    if (!base::parse(buffer, &client_hello))
    {
        onFailed(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Encryption: " << client_hello.encryption();
    LOG(LS_INFO) << "Identify: " << client_hello.identify();

    if (!(client_hello.encryption() & proto::ENCRYPTION_AES256_GCM) &&
        !(client_hello.encryption() & proto::ENCRYPTION_CHACHA20_POLY1305))
    {
        // No encryption methods supported.
        onFailed(FROM_HERE);
        return;
    }

    identify_ = client_hello.identify();
    switch (identify_)
    {
        // SRP is always supported.
        case proto::IDENTIFY_SRP:
            break;

        case proto::IDENTIFY_ANONYMOUS:
        {
            // If anonymous method is not allowed.
            if (anonymous_access_ != AnonymousAccess::ENABLE)
            {
                onFailed(FROM_HERE);
                return;
            }
        }
        break;

        default:
        {
            // Unsupported identication method.
            onFailed(FROM_HERE);
            return;
        }
        break;
    }

    proto::ServerHello server_hello;

    if (key_pair_.isValid())
    {
        decrypt_iv_ = base::fromStdString(client_hello.iv());
        if (decrypt_iv_.empty())
        {
            onFailed(FROM_HERE);
            return;
        }

        base::ByteArray peer_public_key = base::fromStdString(client_hello.public_key());
        if (peer_public_key.empty())
        {
            onFailed(FROM_HERE);
            return;
        }

        base::ByteArray temp = key_pair_.sessionKey(peer_public_key);
        if (temp.empty())
        {
            onFailed(FROM_HERE);
            return;
        }

        session_key_ = base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, temp);
        if (session_key_.empty())
        {
            onFailed(FROM_HERE);
            return;
        }

        DCHECK(!encrypt_iv_.empty());
        server_hello.set_iv(base::toStdString(encrypt_iv_));
    }

    if ((client_hello.encryption() & proto::ENCRYPTION_AES256_GCM) && base::CPUID::hasAesNi())
    {
        LOG(LS_INFO) << "Both sides have hardware support AES. Using AES256 GCM";
        // If both sides of the connection support AES, then method AES256 GCM is the fastest option.
        server_hello.set_encryption(proto::ENCRYPTION_AES256_GCM);
    }
    else
    {
        LOG(LS_INFO) << "Using ChaCha20 Poly1305";
        // Otherwise, we use ChaCha20+Poly1305. This works faster in the absence of hardware
        // support AES.
        server_hello.set_encryption(proto::ENCRYPTION_CHACHA20_POLY1305);
    }

    // Now we are in the authentication phase.
    internal_state_ = InternalState::SEND_SERVER_HELLO;
    encryption_ = server_hello.encryption();

    LOG(LS_INFO) << "Sending: ServerHello";
    channel_->send(base::serialize(server_hello));
}

void ServerAuthenticator::onIdentify(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: Identify";

    proto::SrpIdentify identify;
    if (!base::parse(buffer, &identify))
    {
        onFailed(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Username:" << identify.username();

    user_name_ = base::utf16FromUtf8(identify.username());
    if (user_name_.empty())
    {
        onFailed(FROM_HERE);
        return;
    }

    do
    {
        const User& user = user_list_->find(user_name_);
        if (user.isValid() && (user.flags & User::ENABLED))
        {
            session_types_ = user.sessions;

            std::optional<base::SrpNgPair> Ng_pair = base::pairByGroup(user.group);
            if (Ng_pair.has_value())
            {
                N_ = base::BigNum::fromStdString(Ng_pair->first);
                g_ = base::BigNum::fromStdString(Ng_pair->second);
                s_ = base::BigNum::fromByteArray(user.salt);
                v_ = base::BigNum::fromByteArray(user.verifier);
                break;
            }
            else
            {
                LOG(LS_ERROR) << "User '" << user.name << "' has an invalid SRP group";
            }
        }

        session_types_ = 0;

        base::GenericHash hash(base::GenericHash::BLAKE2b512);
        hash.addData(user_list_->seedKey());
        hash.addData(identify.username());

        N_ = base::BigNum::fromStdString(base::kSrpNgPair_8192.first);
        g_ = base::BigNum::fromStdString(base::kSrpNgPair_8192.second);
        s_ = base::BigNum::fromByteArray(hash.result());
        v_ = base::SrpMath::calc_v(user_name_, user_list_->seedKey(), s_, N_, g_);
    }
    while (false);

    b_ = base::BigNum::fromByteArray(base::Random::byteArray(128)); // 1024 bits.
    B_ = base::SrpMath::calc_B(b_, N_, g_, v_);

    if (!N_.isValid() || !g_.isValid() || !s_.isValid() || !B_.isValid())
    {
        onFailed(FROM_HERE);
        return;
    }

    internal_state_ = InternalState::SEND_SERVER_KEY_EXCHANGE;
    encrypt_iv_ = base::Random::byteArray(kIvSize);

    proto::SrpServerKeyExchange server_key_exchange;

    server_key_exchange.set_number(N_.toStdString());
    server_key_exchange.set_generator(g_.toStdString());
    server_key_exchange.set_salt(s_.toStdString());
    server_key_exchange.set_b(B_.toStdString());
    server_key_exchange.set_iv(base::toStdString(encrypt_iv_));

    LOG(LS_INFO) << "Sending: ServerKeyExchange";
    channel_->send(base::serialize(server_key_exchange));
}

void ServerAuthenticator::onClientKeyExchange(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: ClientKeyExchange";

    proto::SrpClientKeyExchange client_key_exchange;
    if (!base::parse(buffer, &client_key_exchange))
    {
        onFailed(FROM_HERE);
        return;
    }

    A_ = base::BigNum::fromStdString(client_key_exchange.a());
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

    switch (encryption_)
    {
        // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
        case proto::ENCRYPTION_AES256_GCM:
        case proto::ENCRYPTION_CHACHA20_POLY1305:
        {
            base::GenericHash hash(base::GenericHash::BLAKE2s256);

            if (!session_key_.empty())
                hash.addData(session_key_);
            hash.addData(srp_key);

            session_key_ = hash.result();
        }
        break;

        default:
        {
            onFailed(FROM_HERE);
            return;
        }
        break;
    }

    if (!onSessionKeyChanged())
        return;

    internal_state_ = InternalState::SEND_SESSION_CHALLENGE;
    doSessionChallenge();
}

void ServerAuthenticator::doSessionChallenge()
{
    proto::SessionChallenge session_challenge;
    session_challenge.set_session_types(session_types_);

    proto::Version* version = session_challenge.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);

#if defined(OS_WIN)
    session_challenge.set_os_type(proto::OS_TYPE_WINDOWS);
#else
#error Not implemented
#endif

    session_challenge.set_computer_name(base::SysInfo::computerName());
    session_challenge.set_cpu_cores(base::SysInfo::processorCores());

    LOG(LS_INFO) << "Sending: SessionChallenge";
    channel_->send(base::serialize(session_challenge));
}

void ServerAuthenticator::onSessionResponse(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: SessionResponse";

    // Stop receiving incoming messages.
    channel_->pause();
    channel_->setListener(nullptr);

    proto::SessionResponse session_response;
    if (!base::parse(buffer, &session_response))
    {
        onFailed(FROM_HERE);
        return;
    }

    const proto::Version& version = session_response.version();
    peer_version_ = base::Version(version.major(), version.minor(), version.patch());

    LOG(LS_INFO) << "Client Session Type: " << session_response.session_type();
    LOG(LS_INFO) << "Client Version: " << peer_version_;
    LOG(LS_INFO) << "Client Name: " << session_response.computer_name();
    LOG(LS_INFO) << "Client OS: " << osTypeToString(session_response.os_type());
    LOG(LS_INFO) << "Client CPU Cores: " << session_response.cpu_cores();

    base::BitSet<uint32_t> session_type = session_response.session_type();
    if (session_type.count() != 1)
    {
        onFailed(FROM_HERE);
        return;
    }

    session_type_ = session_type.value();
    if (!(session_types_ & session_type_))
    {
        onFailed(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Authentication completed successfully for " << channel_->peerAddress();

    timer_.stop();

    // Authentication completed successfully.
    state_ = State::SUCCESS;

    // Notify of completion.
    delegate_->onComplete();
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

bool ServerAuthenticator::onSessionKeyChanged()
{
    LOG(LS_INFO) << "Session key changed";

    std::unique_ptr<base::MessageEncryptor> encryptor;
    std::unique_ptr<base::MessageDecryptor> decryptor;

    if (encryption_ == proto::ENCRYPTION_AES256_GCM)
    {
        encryptor = base::MessageEncryptorOpenssl::createForAes256Gcm(
            session_key_, encrypt_iv_);
        decryptor = base::MessageDecryptorOpenssl::createForAes256Gcm(
            session_key_, decrypt_iv_);
    }
    else
    {
        DCHECK_EQ(encryption_, proto::ENCRYPTION_CHACHA20_POLY1305);

        encryptor = base::MessageEncryptorOpenssl::createForChaCha20Poly1305(
            session_key_, encrypt_iv_);
        decryptor = base::MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key_, decrypt_iv_);
    }

    if (!encryptor || !decryptor)
    {
        onFailed(FROM_HERE);
        return false;
    }

    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));
    return true;
}

base::ByteArray ServerAuthenticator::createSrpKey()
{
    if (!base::SrpMath::verify_A_mod_N(A_, N_))
    {
        LOG(LS_ERROR) << "SrpMath::verify_A_mod_N failed";
        return base::ByteArray();
    }

    base::BigNum u = base::SrpMath::calc_u(A_, B_, N_);
    base::BigNum server_key = base::SrpMath::calcServerKey(A_, v_, u, b_, N_);

    return server_key.toByteArray();
}

} // namespace peer
