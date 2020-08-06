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

#include "peer/client_authenticator.h"

#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/crypto/srp_constants.h"
#include "base/crypto/srp_math.h"
#include "base/strings/unicode.h"
#include "build/version.h"

namespace peer {

namespace {

const size_t kIvSize = 12; // 12 bytes.

bool verifyNg(std::string_view N, std::string_view g)
{
    switch (N.size())
    {
        case 512: // 4096 bit
        {
            if (N != base::kSrpNgPair_4096.first || g != base::kSrpNgPair_4096.second)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (N != base::kSrpNgPair_6144.first || g != base::kSrpNgPair_6144.second)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (N != base::kSrpNgPair_8192.first || g != base::kSrpNgPair_8192.second)
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

ClientAuthenticator::ClientAuthenticator(std::shared_ptr<base::TaskRunner> task_runner)
    : Authenticator(std::move(task_runner))
{
    // Nothing
}

ClientAuthenticator::~ClientAuthenticator() = default;

void ClientAuthenticator::setPeerPublicKey(const base::ByteArray& public_key)
{
    peer_public_key_ = public_key;
}

void ClientAuthenticator::setIdentify(proto::Identify identify)
{
    identify_ = identify;
}

void ClientAuthenticator::setUserName(std::u16string_view username)
{
    username_ = username;
}

void ClientAuthenticator::setPassword(std::u16string_view password)
{
    password_ = password;
}

void ClientAuthenticator::setSessionType(uint32_t session_type)
{
    session_type_ = session_type;
}

bool ClientAuthenticator::onStarted()
{
    internal_state_ = InternalState::SEND_CLIENT_HELLO;
    sendClientHello();
    return true;
}

void ClientAuthenticator::onReceived(const base::ByteArray& buffer)
{
    switch (internal_state_)
    {
        case InternalState::READ_SERVER_HELLO:
        {
            if (readServerHello(buffer))
            {
                if (identify_ == proto::IDENTIFY_ANONYMOUS)
                {
                    internal_state_ = InternalState::READ_SESSION_CHALLENGE;
                }
                else
                {
                    internal_state_ = InternalState::SEND_IDENTIFY;
                    sendIdentify();
                }
            }
        }
        break;

        case InternalState::READ_SERVER_KEY_EXCHANGE:
        {
            if (readServerKeyExchange(buffer))
            {
                internal_state_ = InternalState::SEND_CLIENT_KEY_EXCHANGE;
                sendClientKeyExchange();
            }
        }
        break;

        case InternalState::READ_SESSION_CHALLENGE:
        {
            if (readSessionChallenge(buffer))
            {
                internal_state_ = InternalState::SEND_SESSION_RESPONSE;
                sendSessionResponse();
            }
        }
        break;

        default:
        {
            NOTREACHED();
        }
        break;
    }
}

void ClientAuthenticator::onWritten()
{
    switch (internal_state_)
    {
        case InternalState::SEND_CLIENT_HELLO:
        {
            LOG(LS_INFO) << "Sended: ClientHello";
            internal_state_ = InternalState::READ_SERVER_HELLO;
        }
        break;

        case InternalState::SEND_IDENTIFY:
        {
            LOG(LS_INFO) << "Sended: Identify";
            internal_state_ = InternalState::READ_SERVER_KEY_EXCHANGE;
        }
        break;

        case InternalState::SEND_CLIENT_KEY_EXCHANGE:
        {
            LOG(LS_INFO) << "Sended: ClientKeyExchange";
            internal_state_ = InternalState::READ_SESSION_CHALLENGE;
            if (!onSessionKeyChanged())
            {
                finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
                return;
            }
        }
        break;

        case InternalState::SEND_SESSION_RESPONSE:
        {
            LOG(LS_INFO) << "Sended: SessionResponse";
            finish(FROM_HERE, ErrorCode::SUCCESS);
        }
        break;

        default:
            break;
    }
}

void ClientAuthenticator::sendClientHello()
{
    // We do not allow anonymous connections without a public key.
    if (identify_ == proto::IDENTIFY_ANONYMOUS && peer_public_key_.empty())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::ClientHello client_hello;

    uint32_t encryption = proto::ENCRYPTION_CHACHA20_POLY1305;

    if (base::CPUID::hasAesNi())
        encryption |= proto::ENCRYPTION_AES256_GCM;

    client_hello.set_encryption(encryption);
    client_hello.set_identify(identify_);

    if (!peer_public_key_.empty())
    {
        encrypt_iv_ = base::Random::byteArray(kIvSize);
        if (encrypt_iv_.empty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        base::KeyPair key_pair = base::KeyPair::create(base::KeyPair::Type::X25519);
        if (!key_pair.isValid())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        base::ByteArray temp = key_pair.sessionKey(peer_public_key_);
        if (temp.empty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        session_key_ = base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, temp);
        if (session_key_.empty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        base::ByteArray public_key = key_pair.publicKey();
        if (public_key.empty())
        {
            finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        client_hello.set_public_key(base::toStdString(public_key));
        client_hello.set_iv(base::toStdString(encrypt_iv_));
    }

    LOG(LS_INFO) << "Sending: ClientHello";
    sendMessage(client_hello);
}

bool ClientAuthenticator::readServerHello(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: ServerHello";

    proto::ServerHello server_hello;
    if (!base::parse(buffer, &server_hello))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    LOG(LS_INFO) << "Encryption: " << server_hello.encryption();

    encryption_ = server_hello.encryption();
    switch (encryption_)
    {
        case proto::ENCRYPTION_AES256_GCM:
        case proto::ENCRYPTION_CHACHA20_POLY1305:
            break;

        default:
            finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            return false;
    }

    decrypt_iv_ = base::fromStdString(server_hello.iv());

    if (session_key_.empty() != decrypt_iv_.empty())
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!session_key_.empty() && !onSessionKeyChanged())
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return false;
    }

    return true;
}

void ClientAuthenticator::sendIdentify()
{
    proto::SrpIdentify identify;
    identify.set_username(base::utf8FromUtf16(username_));

    LOG(LS_INFO) << "Sending: Identify";
    sendMessage(identify);
}

bool ClientAuthenticator::readServerKeyExchange(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: ServerKeyExchange";

    proto::SrpServerKeyExchange server_key_exchange;
    if (!base::parse(buffer, &server_key_exchange))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (server_key_exchange.salt().size() < 64 || server_key_exchange.b().size() < 128)
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!verifyNg(server_key_exchange.number(), server_key_exchange.generator()))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    N_ = base::BigNum::fromStdString(server_key_exchange.number());
    g_ = base::BigNum::fromStdString(server_key_exchange.generator());
    s_ = base::BigNum::fromStdString(server_key_exchange.salt());
    B_ = base::BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = base::fromStdString(server_key_exchange.iv());

    a_ = base::BigNum::fromByteArray(base::Random::byteArray(128)); // 1024 bits.
    A_ = base::SrpMath::calc_A(a_, N_, g_);
    encrypt_iv_ = base::Random::byteArray(kIvSize);

    if (!base::SrpMath::verify_B_mod_N(B_, N_))
    {
        LOG(LS_WARNING) << "Invalid B or N";
        return false;
    }

    base::BigNum u = base::SrpMath::calc_u(A_, B_, N_);
    base::BigNum x = base::SrpMath::calc_x(s_, username_, password_);
    base::BigNum key = base::SrpMath::calcClientKey(N_, B_, g_, x, a_, u);
    if (!key.isValid())
    {
        LOG(LS_WARNING) << "Empty encryption key generated";
        return false;
    }

    // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
    base::GenericHash hash(base::GenericHash::BLAKE2s256);

    if (!session_key_.empty())
        hash.addData(session_key_);
    hash.addData(key.toByteArray());

    session_key_ = hash.result();
    return true;
}

void ClientAuthenticator::sendClientKeyExchange()
{
    proto::SrpClientKeyExchange client_key_exchange;
    client_key_exchange.set_a(A_.toStdString());
    client_key_exchange.set_iv(base::toStdString(encrypt_iv_));

    LOG(LS_INFO) << "Sending: ClientKeyExchange";
    sendMessage(client_key_exchange);
}

bool ClientAuthenticator::readSessionChallenge(const base::ByteArray& buffer)
{
    LOG(LS_INFO) << "Received: SessionChallenge";

    proto::SessionChallenge challenge;
    if (!base::parse(buffer, &challenge))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!(challenge.session_types() & session_type_))
    {
        finish(FROM_HERE, ErrorCode::SESSION_DENIED);
        return false;
    }

    setPeerVersion(challenge.version());

    LOG(LS_INFO) << "Server Version: " << peerVersion();
    LOG(LS_INFO) << "Server Name: " << challenge.computer_name();
    LOG(LS_INFO) << "Server OS: " << osTypeToString(challenge.os_type());
    LOG(LS_INFO) << "Server CPU Cores: " << challenge.cpu_cores();

    return true;
}

void ClientAuthenticator::sendSessionResponse()
{
    proto::SessionResponse response;
    response.set_session_type(session_type_);

    proto::Version* version = response.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);

#if defined(OS_WIN)
    response.set_os_type(proto::OS_TYPE_WINDOWS);
#else
#error Not implemented
#endif

    response.set_computer_name(base::SysInfo::computerName());
    response.set_cpu_cores(base::SysInfo::processorCores());

    LOG(LS_INFO) << "Sending: SessionResponse";
    sendMessage(response);
}

} // namespace peer
