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

#include "base/peer/client_authenticator.h"

#include <QSysInfo>

#include "base/cpuid_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "base/version_constants.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/srp_math.h"
#include "proto/key_exchange.h"

namespace {

const size_t kIvSize = 12; // 12 bytes.

//--------------------------------------------------------------------------------------------------
bool verifyNg(std::string_view N, std::string_view g)
{
    switch (N.size())
    {
        case 512: // 4096 bit
        {
            if (N != SrpMath::kNgPair_4096.first || g != SrpMath::kNgPair_4096.second)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (N != SrpMath::kNgPair_6144.first || g != SrpMath::kNgPair_6144.second)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (N != SrpMath::kNgPair_8192.first || g != SrpMath::kNgPair_8192.second)
                return false;
        }
        break;

        // We do not allow groups less than 512 bytes (4096 bits).
        default:
            return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientAuthenticator::ClientAuthenticator(QObject* parent)
    : Authenticator(parent)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientAuthenticator::~ClientAuthenticator()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setPeerPublicKey(const QByteArray& public_key)
{
    CLOG(INFO) << "Public key assigned:" << public_key.size();
    peer_public_key_ = public_key;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setIdentify(proto::key_exchange::Identify identify)
{
    CLOG(INFO) << "Identify assigned:" << identify;
    identify_ = identify;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setUserName(const QString& username)
{
    CLOG(INFO) << "User name assigned:" << username;
    username_ = username;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setPassword(const QString& password)
{
    CLOG(INFO) << "Password assigned";
    password_ = password;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setSessionType(quint32 session_type)
{
    CLOG(INFO) << "Session type assigned:" << session_type;
    session_type_ = session_type;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setDisplayName(const QString& display_name)
{
    CLOG(INFO) << "Display name assigned:" << display_name;
    display_name_ = display_name;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::setProbe(bool probe)
{
    CLOG(INFO) << "Probe flag assigned:" << probe;
    is_probe_ = probe;
}

//--------------------------------------------------------------------------------------------------
bool ClientAuthenticator::onStarted()
{
    sendClientHello();
    return true;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::onReceived(const QByteArray& buffer)
{
    switch (internal_state_)
    {
        case InternalState::READ_SERVER_HELLO:
        {
            if (readServerHello(buffer))
            {
                if (identify_ == proto::key_exchange::IDENTIFY_ANONYMOUS)
                    internal_state_ = InternalState::READ_SESSION_CHALLENGE;
                else
                    sendIdentify();
            }
        }
        break;

        case InternalState::READ_SERVER_KEY_EXCHANGE:
        {
            if (readServerKeyExchange(buffer))
                sendClientKeyExchange();
        }
        break;

        case InternalState::READ_SESSION_CHALLENGE:
        {
            if (readSessionChallenge(buffer))
                sendSessionResponse();
        }
        break;

        default:
        {
            NOTREACHED();
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
QByteArray ClientAuthenticator::keyLabel(Direction direction) const
{
    return direction == Direction::ENCRYPT ?
        QByteArrayLiteral("aspia/v1/c2s") : QByteArrayLiteral("aspia/v1/s2c");
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::sendClientHello()
{
    // We do not allow anonymous connections without a public key.
    if (identify_ == proto::key_exchange::IDENTIFY_ANONYMOUS && peer_public_key_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    // SRP authentication uses its own key exchange and must never combine with a public key.
    if (identify_ == proto::key_exchange::IDENTIFY_SRP && !peer_public_key_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::key_exchange::ClientHello client_hello;

    quint32 encryption = proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305;

#if defined(Q_PROCESSOR_X86)
    if (CpuidUtil::hasAesNi())
        encryption |= proto::key_exchange::ENCRYPTION_AES256_GCM;
#endif

    client_hello.set_encryption(encryption);
    client_hello.set_identify(identify_);

    if (!peer_public_key_.isEmpty())
    {
        encrypt_iv_ = Random::byteArray(kIvSize);
        if (encrypt_iv_.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
        if (!key_pair.isValid())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        QByteArray x25519_secret = key_pair.sessionKey(peer_public_key_);
        if (x25519_secret.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        // Mix the X25519 shared secret first, before ClientHello bytes. Server applies the same
        // order after parsing ClientHello and deriving the secret from it.
        appendTranscript(x25519_secret);
        memZero(&x25519_secret);

        QByteArray public_key = key_pair.publicKey();
        if (public_key.isEmpty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        client_hello.set_public_key(public_key.toStdString());
        client_hello.set_iv(encrypt_iv_.toStdString());
    }

    // Remove this after support for versions below 3.0.0 ends.
    if (kMinimumSupportedVersion < kVersion_3_0_0)
    {
        proto::peer::Version* version = client_hello.mutable_version();
        version->set_major(ASPIA_VERSION_MAJOR);
        version->set_minor(ASPIA_VERSION_MINOR);
        version->set_patch(ASPIA_VERSION_PATCH);
    }

    QByteArray message = serialize(client_hello);
    appendTranscript(message);

    CLOG(INFO) << "Sending: ClientHello (" << message.size() << ")";
    emit sig_outgoingMessage(message, false);
    internal_state_ = InternalState::READ_SERVER_HELLO;
}

//--------------------------------------------------------------------------------------------------
bool ClientAuthenticator::readServerHello(const QByteArray& buffer)
{
    CLOG(INFO) << "Received: ServerHello (" << buffer.size() << ")";
    appendTranscript(buffer);

    proto::key_exchange::ServerHello server_hello;
    if (!parse(buffer, &server_hello))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    CLOG(INFO) << "Encryption:" << server_hello.encryption();

    encryption_ = server_hello.encryption();
    switch (encryption_)
    {
        case proto::key_exchange::ENCRYPTION_AES256_GCM:
        case proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305:
            break;

        default:
            finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            return false;
    }

    decrypt_iv_ = QByteArray::fromStdString(server_hello.iv());

    // ServerHello.iv must be present in ANONYMOUS mode (server's IV for the X25519-derived
    // session key) and absent in SRP mode (where the IV arrives later in SrpServerKeyExchange).
    // Anything else is a protocol violation.
    const bool is_anonymous = (identify_ == proto::key_exchange::IDENTIFY_ANONYMOUS);
    if (is_anonymous == decrypt_iv_.isEmpty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (is_anonymous)
    {
        // ANONYMOUS path: transcript hash now contains x25519_secret || ClientHello || ServerHello.
        // That is the master from which sessionKey() derives per-direction keys.
        CLOG(INFO) << "Session key is ready";
        setSessionKeyReady();
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::sendIdentify()
{
    proto::key_exchange::SrpIdentify identify;
    identify.set_username(username_.toStdString());

    QByteArray message = serialize(identify);

    CLOG(INFO) << "Sending: Identify (" << message.size() << ")";
    appendTranscript(message);
    emit sig_outgoingMessage(message, false);
    internal_state_ = InternalState::READ_SERVER_KEY_EXCHANGE;
}

//--------------------------------------------------------------------------------------------------
bool ClientAuthenticator::readServerKeyExchange(const QByteArray& buffer)
{
    CLOG(INFO) << "Received: ServerKeyExchange (" << buffer.size() << ")";
    appendTranscript(buffer);

    proto::key_exchange::SrpServerKeyExchange server_key_exchange;
    if (!parse(buffer, &server_key_exchange))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (server_key_exchange.salt().empty() || server_key_exchange.b().empty())
    {
        CLOG(ERROR) << "Salt size:" << server_key_exchange.salt().size();
        CLOG(ERROR) << "B size:" << server_key_exchange.b().size();

        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!verifyNg(server_key_exchange.number(), server_key_exchange.generator()))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    N_ = BigNum::fromStdString(server_key_exchange.number());
    g_ = BigNum::fromStdString(server_key_exchange.generator());
    s_ = BigNum::fromStdString(server_key_exchange.salt());
    B_ = BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = QByteArray::fromStdString(server_key_exchange.iv());

    a_ = BigNum::fromByteArray(Random::byteArray(128)); // 1024 bits.
    A_ = SrpMath::calc_A(a_, N_, g_);
    encrypt_iv_ = Random::byteArray(kIvSize);

    if (!SrpMath::verify_B_mod_N(B_, N_))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    // SRP key computation is deferred to sendClientKeyExchange so that on both sides the
    // transcript hash absorbs ClientKeyExchange before the SRP key is mixed in.
    return true;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::sendClientKeyExchange()
{
    BigNum u = SrpMath::calc_u(A_, B_, N_);
    BigNum x = SrpMath::calc_x(s_, username_, password_);
    BigNum key = SrpMath::calcClientKey(N_, B_, g_, x, a_, u);
    if (!key.isValid())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    proto::key_exchange::SrpClientKeyExchange client_key_exchange;
    client_key_exchange.set_a(A_.toStdString());
    client_key_exchange.set_iv(encrypt_iv_.toStdString());

    QByteArray message = serialize(client_key_exchange);

    CLOG(INFO) << "Sending: ClientKeyExchange (" << message.size() << ")";
    appendTranscript(message);
    emit sig_outgoingMessage(message, false);
    internal_state_ = InternalState::READ_SESSION_CHALLENGE;

    // Mix the SRP key after SrpClientKeyExchange. Transcript now covers ClientHello ||
    // ServerHello || SrpIdentify || SrpServerKeyExchange || SrpClientKeyExchange || srp_key.
    // That is the master from which sessionKey() derives per-direction keys.
    QByteArray srp_raw_key = key.toByteArray();
    appendTranscript(srp_raw_key);
    memZero(&srp_raw_key);

    CLOG(INFO) << "Session key is ready";
    setSessionKeyReady();
}

//--------------------------------------------------------------------------------------------------
bool ClientAuthenticator::readSessionChallenge(const QByteArray& buffer)
{
    CLOG(INFO) << "Received: SessionChallenge (" << buffer.size() << ")";

    proto::key_exchange::SessionChallenge challenge;
    if (!parse(buffer, &challenge))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!(challenge.session_types() & session_type_))
    {
        finish(FROM_HERE, ErrorCode::SESSION_DENIED);
        return false;
    }

    setPeerOsName(QString::fromStdString(challenge.os_name()));
    setPeerComputerName(QString::fromStdString(challenge.computer_name()));
    setPeerArch(QString::fromStdString(challenge.arch()));
    setPeerDisplayName(QString::fromStdString(challenge.display_name()));
    setPeerVersion(challenge.version());

    CLOG(INFO) << "Server (version:" << peerVersion().toString() << "name:" << peerComputerName()
               << "os:" << peerOsName() << "cores:" << challenge.cpu_cores()
               << "arch:" << peerArch() << "display_name:" << peerDisplayName() << ")";

    if (peerVersion() < kMinimumSupportedVersion)
    {
        finish(FROM_HERE, ErrorCode::VERSION_ERROR);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ClientAuthenticator::sendSessionResponse()
{
    proto::key_exchange::SessionResponse response;
    response.set_session_type(session_type_);

    proto::peer::Version* version = response.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);
    version->set_revision(GIT_COMMIT_COUNT);

    response.set_os_name(SysInfo::operatingSystemName().toStdString());
    response.set_computer_name(SysInfo::computerName().toStdString());
    response.set_cpu_cores(static_cast<quint32>(SysInfo::processorThreads()));
    response.set_display_name(display_name_.toStdString());
    response.set_arch(QSysInfo::buildCpuArchitecture().toStdString());
    response.set_probe(is_probe_);

    QByteArray message = serialize(response);

    CLOG(INFO) << "Sending: SessionResponse (" << message.size() << ")";
    emit sig_outgoingMessage(message, true);
    finish(FROM_HERE, ErrorCode::SUCCESS);
}
