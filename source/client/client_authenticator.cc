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

#include "client/client_authenticator.h"

#include "base/cpuid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/unicode.h"
#include "common/message_serialization.h"
#include "crypto/message_decryptor_openssl.h"
#include "crypto/message_encryptor_openssl.h"
#include "crypto/generic_hash.h"
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"
#include "net/network_channel.h"

namespace client {

namespace {

bool verifyNg(std::string_view N, std::string_view g)
{
    switch (N.size())
    {
        case 512: // 4096 bit
        {
            if (N != crypto::kSrpNgPair_4096.first || g != crypto::kSrpNgPair_4096.second)
                return false;
        }
        break;

        case 768: // 6144 bit
        {
            if (N != crypto::kSrpNgPair_6144.first || g != crypto::kSrpNgPair_6144.second)
                return false;
        }
        break;

        case 1024: // 8192 bit
        {
            if (N != crypto::kSrpNgPair_8192.first || g != crypto::kSrpNgPair_8192.second)
                return false;
        }
        break;

        // We do not allow groups less than 512 bytes (4096 bits).
        default:
            return false;
    }

    return true;
}

std::unique_ptr<crypto::MessageEncryptor> createMessageEncryptor(
    proto::Method method, const base::ByteArray& key, const base::ByteArray& iv)
{
    switch (method)
    {
        case proto::METHOD_SRP_AES256_GCM:
            return crypto::MessageEncryptorOpenssl::createForAes256Gcm(key, iv);

        case proto::METHOD_SRP_CHACHA20_POLY1305:
            return crypto::MessageEncryptorOpenssl::createForChaCha20Poly1305(key, iv);

        default:
            return nullptr;
    }
}

std::unique_ptr<crypto::MessageDecryptor> createMessageDecryptor(
    proto::Method method, const base::ByteArray& key, const base::ByteArray& iv)
{
    switch (method)
    {
        case proto::METHOD_SRP_AES256_GCM:
            return crypto::MessageDecryptorOpenssl::createForAes256Gcm(key, iv);

        case proto::METHOD_SRP_CHACHA20_POLY1305:
            return crypto::MessageDecryptorOpenssl::createForChaCha20Poly1305(key, iv);

        default:
            return nullptr;
    }
}

} // namespace

Authenticator::Authenticator() = default;

Authenticator::~Authenticator() = default;

void Authenticator::setUserName(std::u16string_view username)
{
    username_ = username;
}

const std::u16string& Authenticator::userName() const
{
    return username_;
}

void Authenticator::setPassword(std::u16string_view password)
{
    password_ = password;
}

const std::u16string& Authenticator::password() const
{
    return password_;
}

void Authenticator::setSessionType(proto::SessionType session_type)
{
    session_type_ = session_type;
}

void Authenticator::start(std::unique_ptr<net::Channel> channel, Callback callback)
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

std::unique_ptr<net::Channel> Authenticator::takeChannel()
{
    return std::move(channel_);
}

// static
const char* Authenticator::errorToString(Authenticator::ErrorCode error_code)
{
    switch (error_code)
    {
        case Authenticator::ErrorCode::SUCCESS:
            return "SUCCESS";

        case Authenticator::ErrorCode::NETWORK_ERROR:
            return "NETWORK_ERROR";

        case Authenticator::ErrorCode::PROTOCOL_ERROR:
            return "PROTOCOL_ERROR";

        case Authenticator::ErrorCode::ACCESS_DENIED:
            return "ACCESS_DENIED";

        case Authenticator::ErrorCode::SESSION_DENIED:
            return "SESSION_DENIED";

        default:
            return "UNKNOWN";
    }
}

void Authenticator::onConnected()
{
    // The authenticator receives the channel always in an already connected state.
    NOTREACHED();
}

void Authenticator::onDisconnected(net::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << net::errorToString(error_code);

    ErrorCode result = ErrorCode::NETWORK_ERROR;

    if (error_code == net::ErrorCode::ACCESS_DENIED)
        result = ErrorCode::ACCESS_DENIED;

    finished(FROM_HERE, result);
}

void Authenticator::onMessageReceived(const base::ByteArray& buffer)
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
                std::unique_ptr<crypto::MessageEncryptor> message_encryptor =
                    createMessageEncryptor(method_, key_, encrypt_iv_);
                if (!message_encryptor)
                {
                    finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
                    return;
                }

                // Install the encryptor. Now all sent messages will be encrypted.
                channel_->setEncryptor(std::move(message_encryptor));

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

void Authenticator::onMessageWritten()
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
            std::unique_ptr<crypto::MessageDecryptor> message_decryptor =
                createMessageDecryptor(method_, key_, decrypt_iv_);
            if (!message_decryptor)
            {
                finished(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
                return;
            }

            // After the decryptor is installed, all incoming messages will be decrypted in it.
            // We do not install an encryptor here because the next message should be sent
            // without encryption.
            channel_->setDecryptor(std::move(message_decryptor));

            state_ = State::READ_SESSION_CHALLENGE;
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

void Authenticator::sendClientHello()
{
    uint32_t methods = proto::METHOD_SRP_CHACHA20_POLY1305;

    if (base::CPUID::hasAesNi())
        methods |= proto::METHOD_SRP_AES256_GCM;

    proto::ClientHello client_hello;
    client_hello.set_methods(methods);

    channel_->send(common::serializeMessage(client_hello));
}

bool Authenticator::readServerHello(const base::ByteArray& buffer)
{
    proto::ServerHello server_hello;
    if (!common::parseMessage(buffer, &server_hello))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    method_ = server_hello.method();

    switch (method_)
    {
        case proto::METHOD_SRP_AES256_GCM:
        case proto::METHOD_SRP_CHACHA20_POLY1305:
            break;

        default:
            finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
            return false;
    }

    return true;
}

void Authenticator::sendIdentify()
{
    proto::SrpIdentify identify;
    identify.set_username(base::utf8FromUtf16(username_));

    channel_->send(common::serializeMessage(identify));
}

bool Authenticator::readServerKeyExchange(const base::ByteArray& buffer)
{
    proto::SrpServerKeyExchange server_key_exchange;
    if (!common::parseMessage(buffer, &server_key_exchange))
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

    N_ = crypto::BigNum::fromStdString(server_key_exchange.number());
    g_ = crypto::BigNum::fromStdString(server_key_exchange.generator());
    s_ = crypto::BigNum::fromStdString(server_key_exchange.salt());
    B_ = crypto::BigNum::fromStdString(server_key_exchange.b());
    decrypt_iv_ = base::fromStdString(server_key_exchange.iv());

    a_ = crypto::BigNum::fromByteArray(crypto::Random::byteArray(128)); // 1024 bits.
    A_ = crypto::SrpMath::calc_A(a_, N_, g_);
    encrypt_iv_ = crypto::Random::byteArray(12);

    if (!crypto::SrpMath::verify_B_mod_N(B_, N_))
    {
        LOG(LS_WARNING) << "Invalid B or N";
        return false;
    }

    crypto::BigNum u = crypto::SrpMath::calc_u(A_, B_, N_);
    crypto::BigNum x = crypto::SrpMath::calc_x(s_, username_, password_);
    crypto::BigNum key = crypto::SrpMath::calcClientKey(N_, B_, g_, x, a_, u);
    if (!key.isValid())
    {
        LOG(LS_WARNING) << "Empty encryption key generated";
        return false;
    }

    // AES256-GCM and ChaCha20-Poly1305 requires 256 bit key.
    key_ = crypto::GenericHash::hash(crypto::GenericHash::BLAKE2s256, key.toByteArray());
    return true;
}

void Authenticator::sendClientKeyExchange()
{
    proto::SrpClientKeyExchange client_key_exchange;
    client_key_exchange.set_a(A_.toStdString());
    client_key_exchange.set_iv(base::toStdString(encrypt_iv_));

    channel_->send(common::serializeMessage(client_key_exchange));
}

bool Authenticator::readSessionChallenge(const base::ByteArray& buffer)
{
    proto::SessionChallenge challenge;
    if (!common::parseMessage(buffer, &challenge))
    {
        finished(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return false;
    }

    if (!(challenge.session_types() & session_type_))
    {
        finished(FROM_HERE, ErrorCode::SESSION_DENIED);
        return false;
    }

    const proto::Version& peer_version = challenge.version();
    peer_version_ = base::Version(
        peer_version.major(), peer_version.minor(), peer_version.patch());

    return true;
}

void Authenticator::sendSessionResponse()
{
    proto::SessionResponse response;
    response.set_session_type(session_type_);

    channel_->send(common::serializeMessage(response));
}

void Authenticator::finished(const base::Location& location, ErrorCode error_code)
{
    LOG(LS_INFO) << "Authenticator finished with code: " << errorToString(error_code)
                 << "(" << location.toString() << ")";

    channel_->pause();
    channel_->setListener(nullptr);
    callback_(error_code);
}

} // namespace client
