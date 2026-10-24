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

#include <algorithm>

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QPromise>
#include <QSysInfo>
#include <QTimer>

#include "base/bitset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/security_log.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "base/version_constants.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"
#include "base/crypto/srp_math.h"
#include "base/peer/domain_auth_limiter.h"
#include "base/peer/domain_logon_queue.h"
#include "proto/key_exchange.h"

namespace {

constexpr size_t kIvSize = 12;

// Every failed operating system / domain logon is answered when this time has passed since the
// credentials were received, whatever the reason of the failure (wrong password, unknown or
// disabled account, no mapped group, an error of PAM / LogonUserW). The time of the answer then
// does not tell a wrong password from a correct password of an account without permissions: the
// operating system itself delays some failures (pam_unix waits about 2 seconds after a wrong
// password, but not after a correct one), so a fixed delay added after the logon is not enough.
// The value is larger than the usual delay of pam_unix / pam_faildelay and LogonUserW. If the
// operating system takes longer than this (e.g. an unreachable domain controller), the answer time
// is rounded up to the next multiple of this value, so that it still falls into a few fixed slots
// and does not follow the duration of the logon. Together with the flood guard of the server it also slows
// down online password guessing (and the account lockout it causes in the domain).
// A successful logon is answered immediately.
constexpr std::chrono::milliseconds kDomainAuthFailureTime{ 4000 };

} // namespace

//--------------------------------------------------------------------------------------------------
ServerAuthenticator::ServerAuthenticator(QObject* parent)
    : Authenticator(parent)
{
    CLOG(TRACE) << "Ctor";

    // A logon that is still queued when the handshake ends (timeout, error, closed connection) is
    // not needed anymore: the queue skips it instead of asking the operating system.
    connect(this, &Authenticator::sig_finished, this, [this]()
    {
        cancelDomainLogon();
    });
}

//--------------------------------------------------------------------------------------------------
ServerAuthenticator::~ServerAuthenticator()
{
    CLOG(TRACE) << "Dtor";
    cancelDomainLogon();
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::setPeerAddress(const QString& address)
{
    peer_address_ = address;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::cancelDomainLogon()
{
    if (logon_cancel_)
        logon_cancel_->store(true);
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::setUserList(SharedPointer<UserList> user_list)
{
    user_list_ = std::move(user_list);
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
bool ServerAuthenticator::setIdentityKey(const SecureByteArray& private_key)
{
    // The method must be called before calling start().
    if (state() != State::STOPPED)
    {
        CLOG(ERROR) << "Authenticator not in stopped state";
        return false;
    }

    identity_key_pair_ = KeyPair::fromPrivateKey(private_key);
    if (!identity_key_pair_.isValid())
    {
        CLOG(ERROR) << "Failed to load the identity key";
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

        case InternalState::READ_PASSWORD_CREDENTIALS:
            onPasswordCredentials(buffer);
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

    CLOG(TRACE) << "Identify: " << client_hello.identify();

    if (!(encryption & proto::key_exchange::ENCRYPTION_AES256_GCM))
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

        case proto::key_exchange::IDENTIFY_PASSWORD:
        {
            // The password travels to the server, so the server must prove its identity with its
            // persistent key first.
            if (!identity_key_pair_.isValid())
            {
                CLOG(INFO) << "Password method requested, but the server has no identity key";
                finish(FROM_HERE, ErrorCode::ACCESS_DENIED);
                return;
            }

            if (user_list_)
            {
                domain_authenticator_ = user_list_->domainAuthenticator();
                domain_auth_limiter_ = user_list_->domainAuthLimiter();
            }

            if (!domain_authenticator_)
            {
                CLOG(INFO) << "Password method requested, but it is disabled";
                finish(FROM_HERE, ErrorCode::ACCESS_DENIED);
                return;
            }
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
        // SRP and PASSWORD: server generates an ephemeral X25519 keypair, derives shared with the
        // client's ephemeral pubkey from ClientHello.
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

        if (identify_ == proto::key_exchange::IDENTIFY_PASSWORD)
        {
            // PASSWORD: additionally mix DH(client ephemeral, server persistent). Only the owner
            // of the persistent private key derives the same session key, so the client knows who
            // it sends the password to. Order: ClientHello || x25519_secret || static_secret ||
            // ServerHello.
            // A mode label first (domain separation from SRP and from earlier versions of this
            // method), then the static secret.
            appendTranscript(QByteArray(kPasswordModeLabel));

            SecureByteArray static_secret(identity_key_pair_.sessionKey(client_public_key));
            if (static_secret.isEmpty())
            {
                finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
                return;
            }

            appendTranscript(static_secret.toByteArray());

            const QByteArray host_public_key = identity_key_pair_.publicKey();
            if (host_public_key.isEmpty())
            {
                finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
                return;
            }

            server_hello.set_host_public_key(host_public_key.toStdString());
        }

        // Ephemeral server IV for the upcoming SrpIdentify / SrpServerKeyExchange /
        // SrpClientKeyExchange phase. This is overwritten with a fresh final IV in onIdentify.
        // For PASSWORD it is the final IV.
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

    if (encryption & proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        server_hello.set_encryption(proto::key_exchange::ENCRYPTION_AES256_GCM);
    }
    else
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
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

        case proto::key_exchange::IDENTIFY_PASSWORD:
            internal_state_ = InternalState::READ_PASSWORD_CREDENTIALS;
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

    QString user_name = QString::fromStdString(identify.username());
    if (!User::isValidUserName(user_name))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    user_name_ = identify.username();
    CLOG(TRACE) << "Username:" << user_name;

    QByteArray seed_key;
    User user;

    if (!user_list_)
    {
        CLOG(INFO) << "UserList is nullptr";
        finish(FROM_HERE, ErrorCode::ACCESS_DENIED);
        return;
    }

    user = user_list_->find(user_name);
    seed_key = user_list_->seedKey();

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
    hash.addData(user_name_);

    N_ = BigNum::fromStdString(SrpMath::kNgPair_8192.first);
    g_ = BigNum::fromStdString(SrpMath::kNgPair_8192.second);
    s_ = BigNum::fromByteArray(hash.result());
    v_ = SrpMath::calc_v(user_name, SecureByteArray(seed_key), s_, N_, g_);
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
        // AES256-GCM require a 256 bit key. Mix the SRP key after CKE - transcript now covers full
        // handshake plus SRP key - that is the master from which sessionKey() derives per-direction keys.
        case proto::key_exchange::ENCRYPTION_AES256_GCM:
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
void ServerAuthenticator::onPasswordCredentials(const QByteArray& buffer)
{
    CLOG(TRACE) << "Received: PasswordCredentials (" << buffer.size() << ")";

    // The answer time of a failure is measured from here (see kDomainAuthFailureTime).
    credentials_timer_.start();

    if (buffer.isEmpty() || !domain_authenticator_)
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
        return;
    }

    proto::key_exchange::PasswordCredentials credentials;
    if (!parse(buffer, &credentials))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

    // NUL characters and malformed UTF-8 are refused (see User::isValidCredentialText()).
    const bool names_valid = User::isValidCredentialText(credentials.domain()) &&
                             User::isValidCredentialText(credentials.username());
    const bool password_valid = User::isValidCredentialText(credentials.password());

    if (!names_valid)
    {
        memZero(credentials.mutable_password());
        finishMalformedCredentials("invalid name encoding");
        return;
    }

    QString domain = QString::fromStdString(credentials.domain()).trimmed();
    QString username = QString::fromStdString(credentials.username()).trimmed();
    // Decoded straight into the secure string (the temporary QString is moved, not copied); the
    // UTF-8 bytes in the message are wiped right away.
    SecureString password(QString::fromStdString(credentials.password()));
    memZero(credentials.mutable_password());

    // The domain may come separately or as part of the name ("DOMAIN\user"), never both.
    if (domain.isEmpty())
        DomainAuthenticator::splitAccountName(username, &domain, &username);

    if (domain.contains(QLatin1Char('\\')) || domain.contains(QLatin1Char('@')) ||
        username.contains(QLatin1Char('\\')))
    {
        password.clear();
        finishMalformedCredentials("invalid account name");
        return;
    }

    const QString account = domain.isEmpty() ? username : (domain + QLatin1Char('\\') + username);
    if (!User::isValidDomainUserName(account))
    {
        password.clear();
        finishMalformedCredentials("invalid account name");
        return;
    }

    user_name_ = account.toStdString();

    if (!password_valid)
    {
        password.clear();
        SLOG(ERROR) << "[domain logon failed] user:" << account << "reason: invalid password encoding";
        internal_state_ = InternalState::WAIT_DOMAIN_AUTH;
        finishDomainAuthFailure();
        return;
    }

    if (password.isEmpty() ||
        static_cast<size_t>(password.size()) > User::kMaxDomainPasswordLength)
    {
        SLOG(ERROR) << "[domain logon failed] user:" << account << "reason: invalid password length";
        internal_state_ = InternalState::WAIT_DOMAIN_AUTH;
        finishDomainAuthFailure();
        return;
    }

    // An account or a source in backoff is refused without asking the operating system: the attempt
    // never reaches the lockout counter of the domain. The answer is the usual failure at the usual
    // time, so the client can not tell a blocked attempt from a wrong password.
    //
    // The check and the reservation are one atomic step: while this logon is checked, no other
    // logon of the same account starts, and a source has only a few logons in progress. Otherwise
    // parallel connections would all pass the check before the first failure is recorded.
    auto ticket = std::make_shared<DomainAuthLimiter::Ticket>();
    if (domain_auth_limiter_)
    {
        MilliSeconds blocked_for = MilliSeconds::zero();
        const DomainAuthLimiter::Decision decision =
            domain_auth_limiter_->acquire(account, peer_address_, Clock::now(), ticket.get(),
                                          &blocked_for);
        if (decision != DomainAuthLimiter::Decision::ALLOWED)
        {
            password.clear();
            if (decision == DomainAuthLimiter::Decision::BLOCKED)
            {
                SLOG(ERROR) << "[domain logon failed] user:" << account << "address:" << peer_address_
                            << "reason: too many failed logons, blocked for" << blocked_for.count() << "ms";
            }
            else
            {
                SLOG(ERROR) << "[domain logon failed] user:" << account << "address:" << peer_address_
                            << "reason: another logon of the account or too many logons from the "
                               "address in progress";
            }
            internal_state_ = InternalState::WAIT_DOMAIN_AUTH;
            finishDomainAuthFailure();
            return;
        }
    }

    CLOG(INFO) << "Domain logon requested for" << account << "from" << peer_address_;

    internal_state_ = InternalState::WAIT_DOMAIN_AUTH;

    // The operating system may wait for a domain controller for seconds, so the logon runs on the
    // dedicated logon queue. The watcher is a child of the authenticator: if the connection is gone
    // before the logon completes, the result is simply dropped, and a logon that has not started
    // yet is skipped (see cancelDomainLogon()).
    using Watcher = QFutureWatcher<DomainAuthenticator::Result>;

    auto promise = std::make_shared<QPromise<DomainAuthenticator::Result>>();
    Watcher* watcher = new Watcher(this);

    connect(watcher, &Watcher::finished, this, [this, watcher]()
    {
        DomainAuthenticator::Result result;
        if (watcher->future().resultCount() > 0)
            result = watcher->future().result();

        watcher->deleteLater();
        onDomainAuthFinished(result);
    });

    watcher->setFuture(promise->future());

    std::shared_ptr<const DomainAuthenticator> domain_authenticator = domain_authenticator_;
    logon_cancel_ = std::make_shared<std::atomic_bool>(false);

    // The result is recorded in the limiter right on the pool thread: a client that disconnects
    // while the operating system checks the password must not escape the count.
    std::shared_ptr<DomainAuthLimiter> limiter = domain_auth_limiter_;
    const QString peer_address = peer_address_;

    const bool started = DomainLogonQueue::instance()->start(logon_cancel_,
        [promise, domain_authenticator, limiter, ticket, peer_address, domain, username, password]
        (bool cancelled) mutable
    {
        promise->start();
        if (!cancelled)
        {
            DomainAuthenticator::Result result =
                domain_authenticator->authenticate(domain, username, password, peer_address);

            if (limiter)
            {
                limiter->release(ticket.get(),
                                 result.status == DomainAuthenticator::Status::SUCCESS ?
                                     DomainAuthLimiter::Outcome::SUCCESS :
                                     DomainAuthLimiter::Outcome::FAILURE,
                                 Clock::now(), result.account);
            }

            promise->addResult(result);
        }
        else if (limiter)
        {
            // The operating system was not asked: nothing to count.
            limiter->release(ticket.get(), DomainAuthLimiter::Outcome::CANCELLED, Clock::now());
        }
        password.clear();
        promise->finish();
    });

    // The lambda holds its own copy; wipe this one.
    password.clear();

    if (!started)
    {
        // Too many logons are in progress (a flood): refuse at the usual time without queuing.
        delete watcher;
        logon_cancel_.reset();
        if (limiter)
            limiter->release(ticket.get(), DomainAuthLimiter::Outcome::CANCELLED, Clock::now());

        SLOG(ERROR) << "[domain logon failed] user:" << account << "address:" << peer_address_
                    << "reason: too many logons in progress";
        finishDomainAuthFailure();
    }
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::onDomainAuthFinished(const DomainAuthenticator::Result& result)
{
    // The authenticator may have timed out meanwhile.
    if (state() != State::PENDING || internal_state_ != InternalState::WAIT_DOMAIN_AUTH)
        return;

    const QString account = result.account.isEmpty() ?
        QString::fromStdString(user_name_) : result.account;

    CLOG(INFO) << "Domain logon for" << account << "finished:"
               << DomainAuthenticator::statusToString(result.status)
               << "(sessions:" << result.session_types << ")";

    logon_cancel_.reset();

    switch (result.status)
    {
        case DomainAuthenticator::Status::SUCCESS:
        {
            user_name_ = account.toStdString();
            session_types_ = result.session_types;

            SLOG(INFO) << "[domain logon] user:" << account << "sessions:" << session_types_;

            doSessionChallenge();
        }
        break;

        default:
        {
            // NO_PERMISSIONS (valid credentials, but no mapped group) is answered exactly like a
            // wrong password: otherwise the host would confirm to anyone that a password of any
            // domain account is correct. The security log keeps the real reason.
            SLOG(ERROR) << "[domain logon failed] user:" << account << "reason:"
                        << DomainAuthenticator::statusToString(result.status);

            finishDomainAuthFailure();
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::finishMalformedCredentials(const char* reason)
{
    // A malformed request is answered like any other failure (same time, same error), so that the
    // validation rules can not be probed faster than passwords, and it counts against the source.
    SLOG(ERROR) << "[domain logon failed] address:" << peer_address_ << "reason:" << reason;

    if (domain_auth_limiter_)
        domain_auth_limiter_->addAddressFailure(peer_address_, Clock::now());

    internal_state_ = InternalState::WAIT_DOMAIN_AUTH;
    finishDomainAuthFailure();
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticator::finishDomainAuthFailure()
{
    // All failures are answered at the same time after the credentials were received, so that the
    // time of the answer does not depend on the reason of the failure. The IO thread is not blocked:
    // the answer is sent by a precise timer (a coarse one may be off by 5%).
    const auto elapsed = std::chrono::milliseconds(
        credentials_timer_.isValid() ? credentials_timer_.elapsed() : 0);
    // Round up to the next multiple of kDomainAuthFailureTime (at least one).
    const qint64 step_ms = kDomainAuthFailureTime.count();
    const qint64 slot_count = std::max<qint64>(1, (elapsed.count() + step_ms - 1) / step_ms);
    const std::chrono::milliseconds answer_time(step_ms * slot_count);
    const auto remaining = answer_time - elapsed;

    auto finish_failure = [this]()
    {
        if (state() == State::PENDING && internal_state_ == InternalState::WAIT_DOMAIN_AUTH)
            finish(FROM_HERE, ErrorCode::ACCESS_DENIED);
    };

    if (slot_count > 1)
    {
        CLOG(INFO) << "Logon took longer than the failure answer time (" << elapsed.count()
                   << "ms), answered at" << answer_time.count() << "ms";
    }

    if (remaining.count() <= 0)
    {
        // The logon ended exactly at a slot boundary.
        finish_failure();
        return;
    }

    QTimer::singleShot(remaining, Qt::PreciseTimer, this, finish_failure);
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

    if (!setPeerOsName(response.os_name()) ||
        !setPeerComputerName(response.computer_name()) ||
        !setPeerArch(response.arch()) ||
        !setPeerDisplayName(response.display_name()) ||
        !setPeerVersion(response.version()))
    {
        finish(FROM_HERE, ErrorCode::PROTOCOL_ERROR);
        return;
    }

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
