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

#include "base/net/client_authenticator.h"

#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "base/crypto/message_encryptor_openssl.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/random.h"
#include "base/crypto/srp_constants.h"
#include "base/crypto/srp_math.h"
#include "base/strings/unicode.h"

namespace base {

namespace {

const size_t kIvSize = 12; // 12 bytes.

bool verifyNg(std::string_view N, std::string_view g)
{
    switch (N.size())
    {
        case 512: // 4096 bit
        {
            if (N != kSrpNgPair_4096.first || g != kSrpNgPair_4096.second)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (N != kSrpNgPair_6144.first || g != kSrpNgPair_6144.second)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (N != kSrpNgPair_8192.first || g != kSrpNgPair_8192.second)
                return false;
        }
        break;

        // We do not allow groups less than 512 bytes (4096 bits).
        default:
            return false;
    }

    return true;
}

std::unique_ptr<MessageEncryptor> createMessageEncryptor(
    proto::Encryption encryption, const ByteArray& key, const ByteArray& iv)
{
    switch (encryption)
    {
        case proto::ENCRYPTION_AES256_GCM:
            return MessageEncryptorOpenssl::createForAes256Gcm(key, iv);

        case proto::ENCRYPTION_CHACHA20_POLY1305:
            return MessageEncryptorOpenssl::createForChaCha20Poly1305(key, iv);

        default:
            return nullptr;
    }
}

std::unique_ptr<MessageDecryptor> createMessageDecryptor(
    proto::Encryption encryption, const ByteArray& key, const ByteArray& iv)
{
    switch (encryption)
    {
        case proto::ENCRYPTION_AES256_GCM:
            return MessageDecryptorOpenssl::createForAes256Gcm(key, iv);

        case proto::ENCRYPTION_CHACHA20_POLY1305:
            return MessageDecryptorOpenssl::createForChaCha20Poly1305(key, iv);

        default:
            return nullptr;
    }
}

} // namespace

ClientAuthenticator::ClientAuthenticator() = default;

ClientAuthenticator::~ClientAuthenticator() = default;

void ClientAuthenticator::setPeerPublicKey(const ByteArray& public_key)
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

const std::u16string& ClientAuthenticator::userName() const
{
    return username_;
}

void ClientAuthenticator::setPassword(std::u16string_view password)
{
    password_ = password;
}

const std::u16string& ClientAuthenticator::password() const
{
    return password_;
}

void ClientAuthenticator::setSessionType(uint32_t session_type)
{
    session_type_ = session_type;
}

void ClientAuthenticator::start(std::unique_ptr<NetworkChannel> channel, Callback callback)
{
    channel_ = std::move(channel);
    callback_ = std::move(callback);

    DCHECK(channel_);
    DCHECK(callback_);

    channel_->setListener(this);
    channel_->resume();

    state_ = State::SEND_CLIENT_HELLO;
    sendClientHello();
}

std::unique_ptr<NetworkChannel> ClientAuthenticator::takeChannel()
{
    return std::move(channel_);
}

// static
const char* ClientAuthenticator::errorToString(ClientAuthenticator::ErrorCode error_code)
{
    switch (error_code)
    {
        case ClientAuthenticator::ErrorCode::SUCCESS:
            return "SUCCESS";

        case ClientAuthenticator::ErrorCode::NETWORK_ERROR:
            return "NETWORK_ERROR";

        case ClientAuthenticator::ErrorCode::PROTOCOL_ERROR:
            return "PROTOCOL_ERROR";

        case ClientAuthenticator::ErrorCode::ACCESS_DENIED:
            return "ACCESS_DENIED";

        case ClientAuthenticator::ErrorCode::SESSION_DENIED:
            return "SESSION_DENIED";

        default:
            return "UNKNOWN";
    }
}

void ClientAuthenticator::onConnected()
{
    // The authenticator receives the channel always in an already connected state.
    NOTREACHED();
}

void ClientAuthenticator::onDisconnected(NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << NetworkChannel::errorToString(error_code);

    ErrorCode result = ErrorCode::NETWORK_ERROR;

    if (error_code == NetworkChannel::ErrorCode::ACCESS_DENIED)
        result = ErrorCode::ACCESS_DENIED;

    finished(FROM_HERE, result);
}

void ClientAuthenticator::onMessageReceived(const ByteArray& buffer)
{
    switch (state_)
    {
        case State::READ_SERVER_HELLO:
        {
            if (readServerHello(buffer))
            {
                state_ = State::SEND_IDENTIFY;
                sendIdentify();
            }
        }
        break;

        case State::READ_SERVER_KEY_EXCHANGE:
        {
            if (readServerKeyExchange(buffer))
            {
                state_ = State::SEND_CLIENT_KEY_EXCHANGE;
                sendClientKeyExchange();
            }
        }
        break;

        case State::READ_SESSION_CHALLENGE:
        {
            if (readSessionChallenge(buffer))
            {
                state_ = State::SEND_SESSION_RESPONSE;
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

void ClientAuthenticator::onMessageWritten(size_t /* pending */)
{
    switch (state_)
    {
        case State::SEND_CLIENT_HELLO:
            state_ = State::READ_SERVER_HELLO;
            break;

        case State::SEND_IDENTIFY:
            state_ = State::READ_SERVER_KEY_EXCHANGE;
            break;

        case State::SEND_CLIENT_KEY_EXCHANGE:
        {
            state_ = State::READ_SESSION_CHALLENGE;
            onSessionKeyChanged();
        }
        break;

        case State::SEND_SESSION_RESPONSE:
        {
            state_ = State::FINISHED;
            finished(FROM_HERE, ErrorCode::SUCCESS);
        }
        break;
    }
}

void ClientAuthenticator::onSessionKeyChanged()
{
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
        finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));
}

void ClientAuthenticator::sendClientHello()
{
    // We do not allow anonymous connections without a public key.
    if (identify_ == proto::IDENTIFY_ANONYMOUS && peer_public_key_.empty())
    {
        finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::ClientHello client_hello;

    uint32_t encryption = proto::ENCRYPTION_CHACHA20_POLY1305;

    if (CPUID::hasAesNi())
        encryption |= proto::ENCRYPTION_AES256_GCM;

    client_hello.set_encryption(encryption);
    client_hello.set_identify(identify_);

    if (!peer_public_key_.empty())
    {
        encrypt_iv_ = Random::byteArray(kIvSize);
        if (encrypt_iv_.empty())
        {
            finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
        if (!key_pair.isValid())
        {
            finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        ByteArray temp = key_pair.sessionKey(peer_public_key_);
        if (temp.empty())
        {
            finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        session_key_ = GenericHash::hash(GenericHash::Type::BLAKE2s256, temp);
        if (session_key_.empty())
        {
            finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        ByteArray public_key = key_pair.publicKey();
        if (public_key.empty())
        {
            finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
            return;
        }

        client_hello.set_public_key(toStdString(public_key));
        client_hello.set_iv(toStdString(encrypt_iv_));
    }

    channel_->send(base::serialize(client_hello));
}

bool ClientAuthenticator::readServerHello(const ByteArray& buffer)
{
    proto::ServerHello server_hello;
    if (!base::parse(buffer, &server_hello))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    encryption_ = server_hello.encryption();
    switch (encryption_)
    {
        case proto::ENCRYPTION_AES256_GCM:
        case proto::ENCRYPTION_CHACHA20_POLY1305:
            break;

        default:
            finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            return false;
    }

    decrypt_iv_ = fromStdString(server_hello.iv());

    if (session_key_.empty() != decrypt_iv_.empty())
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!session_key_.empty())
        onSessionKeyChanged();

    return true;
}

void ClientAuthenticator::sendIdentify()
{
    proto::SrpIdentify identify;
    identify.set_username(utf8FromUtf16(username_));
    channel_->send(serialize(identify));
}

bool ClientAuthenticator::readServerKeyExchange(const ByteArray& buffer)
{
    proto::SrpServerKeyExchange server_key_exchange;
    if (!parse(buffer, &server_key_exchange))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (server_key_exchange.salt().size() < 64 || server_key_exchange.b().size() < 128)
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!verifyNg(server_key_exchange.number(), server_key_exchange.generator()))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    N_ = BigNum::fromStdString(server_key_exchange.number());
    g_ = BigNum::fromStdString(server_key_exchange.generator());
    s_ = BigNum::fromStdString(server_key_exchange.salt());
    B_ = BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = fromStdString(server_key_exchange.iv());

    a_ = BigNum::fromByteArray(Random::byteArray(128)); // 1024 bits.
    A_ = SrpMath::calc_A(a_, N_, g_);
    encrypt_iv_ = Random::byteArray(kIvSize);

    if (!SrpMath::verify_B_mod_N(B_, N_))
    {
        LOG(LS_WARNING) << "Invalid B or N";
        return false;
    }

    BigNum u = SrpMath::calc_u(A_, B_, N_);
    BigNum x = SrpMath::calc_x(s_, username_, password_);
    BigNum key = SrpMath::calcClientKey(N_, B_, g_, x, a_, u);
    if (!key.isValid())
    {
        LOG(LS_WARNING) << "Empty encryption key generated";
        return false;
    }

    // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
    GenericHash hash(GenericHash::BLAKE2s256);

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
    client_key_exchange.set_iv(toStdString(encrypt_iv_));

    channel_->send(serialize(client_key_exchange));
}

bool ClientAuthenticator::readSessionChallenge(const ByteArray& buffer)
{
    proto::SessionChallenge challenge;
    if (!parse(buffer, &challenge))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!(challenge.session_types() & session_type_))
    {
        finished(FROM_HERE, ErrorCode::SESSION_DENIED);
        return false;
    }

    const proto::Version& version = challenge.version();
    peer_version_ = Version(version.major(), version.minor(), version.patch());

    return true;
}

void ClientAuthenticator::sendSessionResponse()
{
    proto::SessionResponse response;
    response.set_session_type(session_type_);
    channel_->send(serialize(response));
}

void ClientAuthenticator::finished(const Location& location, ErrorCode error_code)
{
    LOG(LS_INFO) << "Authenticator finished with code: " << errorToString(error_code)
                 << " (" << location.toString() << ")";

    channel_->pause();
    channel_->setListener(nullptr);

    callback_(error_code);
}

} // namespace base
