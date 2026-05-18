//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/peer/server_authenticator.h"

#include <QSysInfo>

#include "base/bitset.h"
#include "base/cpuid_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "base/version_constants.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/srp_math.h"
#include "proto/key_exchange.h"

namespace {

constexpr size_t kIvSize = 12;

} // namespace

//--------------------------------------------------------------------------------------------------
ServerAuthenticator::ServerAuthenticator(QObject* parent)
    : Authenticator(parent)
{
    CLOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ServerAuthenticator::~ServerAuthenticator()
{
    CLOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::setUserList(SharedPointer<UserList> user_list)
{
    user_list_ = std::move(user_list);
    CDCHECK(user_list_);
    CLOG(TRACE) << "User list is assigned";
}

//--------------------------------------------------------------------------------------------------
bool ServerAuthenticator::setPrivateKey(const SecureByteArray& private_key)
{
    // The method must be called before calling start().
    if (state() != State::STOPPED)
    {
        CLOG(ERROR) << "Authenticator not in stopped state";
        return false;
    }

    if (private_key.isEmpty())
    {
        CLOG(ERROR) << "An empty private key is not valid";
        return false;
    }

    key_pair_ = KeyPair::fromPrivateKey(private_key);
    if (!key_pair_.isValid())
    {
        CLOG(ERROR) << "Failed to load private key. Perhaps the key is incorrect";
        return false;
    }

    encrypt_iv_ = Random::byteArray(kIvSize);
    if (encrypt_iv_.isEmpty())
    {
        CLOG(ERROR) << "An empty IV is not valid";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ServerAuthenticator::setAnonymousAccess(AnonymousAccess anonymous_access, quint32 session_types)
{
    // The method must be called before calling start().
    if (state() != State::STOPPED)
    {
        CLOG(ERROR) << "Authenticator not in stopped state";
        return false;
    }

    if (anonymous_access == AnonymousAccess::ENABLE)
    {
        if (!key_pair_.isValid())
        {
            CLOG(ERROR) << "When anonymous access is enabled, a private key must be installed";
            return false;
        }

        if (!session_types)
        {
            CLOG(ERROR) << "When anonymous access is enabled, there must be at least one "
                           "session for anonymous access";
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

//--------------------------------------------------------------------------------------------------
bool ServerAuthenticator::onStarted()
{
    internal_state_ = InternalState::READ_CLIENT_HELLO;

    if (anonymous_access_ == AnonymousAccess::ENABLE)
    {
        // When anonymous access is enabled, a private key must be installed.
        if (!key_pair_.isValid())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return false;
        }

        // When anonymous access is enabled, there must be at least one session for anonymous access.
        if (!session_types_)
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return false;
        }
    }
    else
    {
        // If anonymous access is disabled, then there should not be allowed sessions by default.
        if (session_types_)
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onReceived(const QByteArray& buffer)
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
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray ServerAuthenticator::keyLabel(Direction direction) const
{
    return direction == Direction::ENCRYPT ?
        QByteArrayLiteral("aspia/v1/s2c") : QByteArrayLiteral("aspia/v1/c2s");
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onClientHello(const QByteArray& buffer)
{
    CLOG(TRACE) << "Received: ClientHello (" << buffer.size() << ")";

    if (buffer.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::key_exchange::ClientHello client_hello;
    if (!parse(buffer, &client_hello))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    const quint32 encryption = client_hello.encryption();

    CLOG(TRACE) << "Supported by client:";

    if (encryption & proto::key_exchange::ENCRYPTION_AES256_GCM)
        CLOG(TRACE) << "ENCRYPTION_AES256_GCM";

    if (encryption & proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305)
        CLOG(TRACE) << "ENCRYPTION_CHACHA20_POLY1305";

    CLOG(TRACE) << "Identify: " << client_hello.identify();

    if (!(encryption & proto::key_exchange::ENCRYPTION_AES256_GCM) &&
        !(encryption & proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305))
    {
        // No encryption methods supported.
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    identify_ = client_hello.identify();
    switch (identify_)
    {
        case proto::key_exchange::IDENTIFY_SRP:
            break;

        case proto::key_exchange::IDENTIFY_ANONYMOUS:
            // If anonymous method is not allowed.
            if (anonymous_access_ != AnonymousAccess::ENABLE)
            {
                finish(FROM_HERE, ErrorCode::ACCESS_DENIED);
                return;
            }
            break;

        default:
            // Unsupported identication method.
            finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            return;
    }

    QByteArray client_public_key = QByteArray::fromStdString(client_hello.public_key());
    if (client_public_key.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    decrypt_iv_ = QByteArray::fromStdString(client_hello.iv());
    if (decrypt_iv_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    proto::key_exchange::ServerHello server_hello;

    if (identify_ == proto::key_exchange::IDENTIFY_ANONYMOUS)
    {
        // ANONYMOUS: long-term server keypair derives shared with client's ephemeral pubkey.
        // Order: x25519_secret || ClientHello || ServerHello.
        if (!key_pair_.isValid())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        if (encrypt_iv_.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        SecureByteArray x25519_secret(key_pair_.sessionKey(client_public_key));
        if (x25519_secret.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        appendTranscript(x25519_secret.toByteArray());

        server_hello.set_iv(encrypt_iv_.toStdString());
        appendTranscript(buffer);
    }
    else
    {
        // SRP: server generates an ephemeral X25519 keypair, derives shared with the client's
        // ephemeral pubkey from ClientHello.
        // Order: ClientHello || x25519_secret || ServerHello (mirrors what the client builds, since
        // the client cannot derive the secret until it sees this ServerHello).
        appendTranscript(buffer);

        // Ephemeral X25519 keypair used for the SRP handshake.
        KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
        if (!key_pair.isValid())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        SecureByteArray x25519_secret(key_pair.sessionKey(client_public_key));
        if (x25519_secret.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        appendTranscript(x25519_secret.toByteArray());

        // Ephemeral server IV for the upcoming SrpIdentify / SrpServerKeyExchange /
        // SrpClientKeyExchange phase. This is overwritten with a fresh final IV in onIdentify.
        encrypt_iv_ = Random::byteArray(kIvSize);
        if (encrypt_iv_.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        QByteArray server_public_key = key_pair.publicKey();
        if (server_public_key.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        server_hello.set_public_key(server_public_key.toStdString());
        server_hello.set_iv(encrypt_iv_.toStdString());
    }

    bool has_aes_ni = false;

#if defined(Q_PROCESSOR_X86)
    has_aes_ni = CpuidUtil::hasAesNi();
#endif

    if ((encryption & proto::key_exchange::ENCRYPTION_AES256_GCM) && has_aes_ni)
    {
        CLOG(TRACE) << "Both sides have hardware support AES. Using AES256 GCM";
        // If both sides of the connection support AES, then method AES256 GCM is the fastest option.
        server_hello.set_encryption(proto::key_exchange::ENCRYPTION_AES256_GCM);
    }
    else
    {
        CLOG(TRACE) << "Using ChaCha20+Poly1305";
        // Otherwise, we use ChaCha20+Poly1305. This works faster in the absence of hardware
        // support AES.
        server_hello.set_encryption(proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305);
    }

    encryption_ = server_hello.encryption();

    QByteArray message = serialize(server_hello);
    if (message.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    CLOG(TRACE) << "Sending: ServerHello (" << message.size() << ")";
    appendTranscript(message);
    emit sig_outgoingMessage(message, false);

    // The channel installs its encryptor / decryptor.
    // For SRP this is the ephemeral phase: it protects SrpIdentify / SrpServerKeyExchange /
    // SrpClientKeyExchange.
    // For ANONYMOUS it is the final phase.
    CLOG(TRACE) << "Session key is ready";
    setSessionKeyReady();

    switch (identify_)
    {
        case proto::key_exchange::IDENTIFY_SRP:
            internal_state_ = InternalState::READ_IDENTIFY;
            break;

        case proto::key_exchange::IDENTIFY_ANONYMOUS:
            doSessionChallenge();
            break;

        default:
            finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onIdentify(const QByteArray& buffer)
{
    CLOG(TRACE) << "Received: Identify (" << buffer.size() << ")";

    if (buffer.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    appendTranscript(buffer);

    proto::key_exchange::SrpIdentify identify;
    if (!parse(buffer, &identify))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    user_name_ = QString::fromStdString(identify.username());
    if (user_name_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    CLOG(TRACE) << "Username:" << user_name_;

    QByteArray seed_key;
    User user;

    if (user_list_)
    {
        user = user_list_->find(user_name_);
        seed_key = user_list_->seedKey();
    }
    else
    {
        CLOG(INFO) << "UserList is nullptr";
    }

    if (seed_key.isEmpty())
    {
        CLOG(INFO) << "Empty seed key. Using random 64 bytes";
        seed_key = Random::byteArray(64);
    }

    if (user.isValid())
    {
        CLOG(TRACE) << "User" << user_name_ << "is found (enabled:"
                    << ((user.flags & User::ENABLED) != 0) << ")";
    }
    else
    {
        CLOG(TRACE) << "User" << user_name_ << "is NOT found";
    }

    // Always perform the fake verifier derivation, regardless of whether the user exists.
    // Without this an attacker can distinguish valid from invalid usernames by measuring
    // response latency - the fake path does a BLAKE2b512 + a full modexp (calc_v at 8192 bits)
    // that the valid-user path would otherwise skip. By doing the fake compute unconditionally
    // and then overriding (s, v, N, g) for real users, both paths take the same time up to the
    // final calc_B step.
    GenericHash hash(GenericHash::BLAKE2b512);
    hash.addData(seed_key);
    hash.addData(user_name_.toUtf8());

    N_ = BigNum::fromStdString(SrpMath::kNgPair_8192.first);
    g_ = BigNum::fromStdString(SrpMath::kNgPair_8192.second);
    s_ = BigNum::fromByteArray(hash.result());
    v_ = SrpMath::calc_v(user_name_, SecureByteArray(seed_key), s_, N_, g_);
    session_types_ = 0;

    if (user.isValid() && (user.flags & User::ENABLED))
    {
        std::optional<SrpMath::NgPair> Ng_pair = SrpMath::pairByGroup(user.group);
        if (Ng_pair.has_value())
        {
            N_ = BigNum::fromStdString(Ng_pair->first);
            g_ = BigNum::fromStdString(Ng_pair->second);
            s_ = BigNum::fromByteArray(user.salt);
            v_ = BigNum::fromByteArray(user.verifier);
            session_types_ = user.sessions;
            user_id_ = user.entry_id;
        }
        else
        {
            CLOG(ERROR) << "User" << user.name << "has an invalid SRP group";
        }
    }

    if (!N_.isValid() || !g_.isValid() || !s_.isValid() || !v_.isValid())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    b_ = BigNum::fromByteArray(Random::byteArray(128)); // 1024 bits.
    if (!b_.isValid())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    B_ = SrpMath::calc_B(b_, N_, g_, v_);
    if (!B_.isValid())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    encrypt_iv_ = Random::byteArray(kIvSize);
    if (encrypt_iv_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::key_exchange::SrpServerKeyExchange server_key_exchange;
    server_key_exchange.set_number(N_.toStdString());
    server_key_exchange.set_generator(g_.toStdString());
    server_key_exchange.set_salt(s_.toStdString());
    server_key_exchange.set_b(B_.toStdString());
    server_key_exchange.set_iv(encrypt_iv_.toStdString());

    QByteArray message = serialize(server_key_exchange);
    if (message.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    CLOG(TRACE) << "Sending: ServerKeyExchange (" << message.size() << ")";
    appendTranscript(message);

    // Encrypted under the ephemeral key. Channel rebuilds its encryptor to the final key on
    // the next sig_keyChanged (fired after we receive SrpClientKeyExchange).
    emit sig_outgoingMessage(message, true);
    internal_state_ = InternalState::READ_CLIENT_KEY_EXCHANGE;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onClientKeyExchange(const QByteArray& buffer)
{
    CLOG(TRACE) << "Received: ClientKeyExchange (" << buffer.size() << ")";

    if (buffer.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    appendTranscript(buffer);

    proto::key_exchange::SrpClientKeyExchange client_key_exchange;
    if (!parse(buffer, &client_key_exchange))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    A_ = BigNum::fromStdString(client_key_exchange.a());
    decrypt_iv_ = QByteArray::fromStdString(client_key_exchange.iv());

    if (!A_.isValid() || decrypt_iv_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    SecureByteArray srp_key(createSrpKey());
    if (srp_key.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    switch (encryption_)
    {
        // AES256-GCM and ChaCha20-Poly1305 require a 256 bit key. Mix the SRP key after CKE -
        // transcript now covers full handshake plus SRP key - that is the master from which
        // sessionKey() derives per-direction keys.
        case proto::key_exchange::ENCRYPTION_AES256_GCM:
        case proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305:
            appendTranscript(srp_key.toByteArray());
            break;

        default:
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
    }

    CLOG(TRACE) << "Session key is ready";
    setSessionKeyReady();

    doSessionChallenge();
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::doSessionChallenge()
{
    proto::key_exchange::SessionChallenge session_challenge;
    session_challenge.set_session_types(session_types_);

    proto::peer::Version* version = session_challenge.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);
    version->set_revision(GIT_COMMIT_COUNT);

    session_challenge.set_os_name(SysInfo::operatingSystemName().toStdString());
    session_challenge.set_computer_name(SysInfo::computerName().toStdString());
    session_challenge.set_cpu_cores(static_cast<quint32>(SysInfo::processorThreads()));
    session_challenge.set_arch(QSysInfo::buildCpuArchitecture().toStdString());

    QByteArray message = serialize(session_challenge);
    if (message.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    CLOG(TRACE) << "Sending: SessionChallenge (" << message.size() << ")";
    emit sig_outgoingMessage(message, true);
    internal_state_ = InternalState::READ_SESSION_RESPONSE;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onSessionResponse(const QByteArray& buffer)
{
    CLOG(TRACE) << "Received: SessionResponse (" << buffer.size() << ")";

    if (buffer.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::key_exchange::SessionResponse response;
    if (!parse(buffer, &response))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    setPeerOsName(QString::fromStdString(response.os_name()));
    setPeerComputerName(QString::fromStdString(response.computer_name()));
    setPeerArch(QString::fromStdString(response.arch()));
    setPeerDisplayName(QString::fromStdString(response.display_name()));
    setPeerVersion(response.version());
    is_probe_ = response.probe();

    CLOG(TRACE) << "Client (session_type:" << response.session_type()
                << "version:" << peerVersion().toString() << "name:" << peerComputerName()
                << "os:" << peerOsName() << "cores:" << response.cpu_cores()
                << "arch:" << peerArch() << "display_name:" << peerDisplayName()
                << "probe:" << is_probe_ << ")";

    if (peerVersion() < kMinimumSupportedVersion)
    {
        finish(FROM_HERE, ErrorCode::VERSION_ERROR);
        return;
    }

    BitSet<quint32> session_type = response.session_type();
    if (session_type.count() != 1)
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    session_type_ = session_type.value();
    if (!(session_types_ & session_type_))
    {
        finish(FROM_HERE, ErrorCode::SESSION_DENIED);
        return;
    }

    // Authentication completed successfully.
    finish(FROM_HERE, ErrorCode::SUCCESS);
}

//--------------------------------------------------------------------------------------------------
QByteArray ServerAuthenticator::createSrpKey()
{
    if (!A_.isValid() || !N_.isValid() || !B_.isValid() || !v_.isValid() || !b_.isValid())
    {
        CLOG(ERROR) << "Invalid parameters";
        return QByteArray();
    }

    if (!SrpMath::verify_A_mod_N(A_, N_))
    {
        CLOG(ERROR) << "SrpMath::verify_A_mod_N failed";
        return QByteArray();
    }

    BigNum u = SrpMath::calc_u(A_, B_, N_);
    if (!u.isValid())
    {
        CLOG(ERROR) << "SrpMath::calc_u failed";
        return QByteArray();
    }

    BigNum server_key = SrpMath::calcServerKey(A_, v_, u, b_, N_);
    if (!server_key.isValid())
    {
        CLOG(ERROR) << "SrpMath::calcServerKey failed";
        return QByteArray();
    }

    return server_key.toByteArray();
}
