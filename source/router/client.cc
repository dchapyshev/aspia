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

#include "router/client.h"

#include <QDateTime>

#include <set>
#include <unordered_map>

#include "base/core_application.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/crypto/totp.h"
#include "base/threading/worker.h"
#include "proto/relay_peer.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"
#include "router/workers/client_worker.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

namespace {

const char kOtpIssuer[] = "Aspia Router";

//--------------------------------------------------------------------------------------------------
qint64 createClientId()
{
    static qint64 last_client_id = 0;
    ++last_client_id;
    return last_client_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Client::Client(TcpChannel* channel, QObject* parent)
    : QObject(parent),
      session_id_(createClientId()),
      tcp_channel_(channel)
{
    CDCHECK(tcp_channel_);
    tcp_channel_->setParent(this);

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
    connect(this, &Client::sig_started, this, &Client::onStarted);
    connect(Worker::current(), &Worker::sig_tick, tcp_channel_, &TcpChannel::tick);

    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);
    emit sig_notifyChanged(ClientWorker::NOTIFY_CLIENTS);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Client::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
const std::string& Client::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Client::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
const std::string& Client::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
const std::string& Client::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
qint64 Client::userId() const
{
    return tcp_channel_->peerUserId();
}

//--------------------------------------------------------------------------------------------------
proto::router::SessionType Client::sessionType() const
{
    return static_cast<proto::router::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
void Client::sendMessage(quint8 channel_id, const QByteArray& message)
{
    tcp_channel_->send(channel_id, message);
}

//--------------------------------------------------------------------------------------------------
void Client::setStunInfo(quint16 port)
{
    stun_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void Client::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_CLIENT)
        return;

    proto::router::ClientToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from client";
        return;
    }

    if (!two_factor_completed_)
    {
        if (message.has_two_factor_response())
            readTwoFactorResponse(message.two_factor_response());
        else
            CLOG(ERROR) << "Unexpected message before 2FA completion";
        return;
    }

    if (message.has_connection_request())
    {
        readConnectionRequest(message.connection_request());
    }
    else if (message.has_check_host_status())
    {
        readCheckHostStatus(message.check_host_status());
    }
    else if (message.has_host_list_request())
    {
        readHostListRequest(message.host_list_request());
    }
    else if (message.has_host_search_request())
    {
        readHostSearchRequest(message.host_search_request());
    }
    else if (message.has_temp_host_list_request())
    {
        readTempHostListRequest(message.temp_host_list_request());
    }
    else if (message.has_workspace_list_request())
    {
        readWorkspaceListRequest(message.workspace_list_request());
    }
    else if (message.has_group_list_request())
    {
        readGroupListRequest(message.group_list_request());
    }
    else if (message.has_change_password_request())
    {
        readChangePasswordRequest(message.change_password_request());
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from client";
    }
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    CLOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Client::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    onSessionMessage(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void Client::onStarted()
{
    doTwoFactorChallenge();
}

//--------------------------------------------------------------------------------------------------
void Client::doTwoFactorChallenge()
{
    proto::router::RouterToClient message;
    proto::router::TwoFactorChallenge* challenge = message.mutable_two_factor_challenge();

    RouterUser user = Database::instance().findUser(userId());
    if (!user.isValid())
    {
        // SRP already validated the user; reaching this branch implies the row vanished
        // between authentication and the 2FA stage.
        CLOG(WARNING) << "Authenticated user" << userName()
                      << "disappeared from database. Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    user_otp_secret_ = user.otp_secret;
    user_otp_counter_ = user.otp_counter;

    if (user_otp_secret_.isEmpty())
    {
        // First login or after admin reset. Generate a tentative secret and hand it to the
        // client; the secret only reaches the database once the user confirms it with a
        // valid code, so an abandoned dialog leaves the user un-enrolled.
        tentative_otp_secret_ = Totp::generateSecret();
        const QString uri = Totp::buildUri(
            kOtpIssuer, QString::fromStdString(userName()), tentative_otp_secret_);

        challenge->set_mode(proto::router::TWO_FACTOR_MODE_ENROLL);
        challenge->set_otpauth_uri(uri.toStdString());
    }
    else
    {
        challenge->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
    }

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readTwoFactorResponse(const proto::router::TwoFactorResponse& response)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool enroll = !tentative_otp_secret_.isEmpty();

    if (!enroll && !response.token().empty())
    {
        // Token path: client presented a previously issued bearer token. Validate by lookup
        // and check that the token's owner matches the user that just passed SRP.
        const std::string_view token = response.token();

        qint64 stored_user_id = 0;
        qint64 token_id = 0;
        if (!Database::instance().findClientDeviceToken(token, &stored_user_id, &token_id) ||
            stored_user_id != userId())
        {
            // The presented token is gone or owned by someone else (revoked, password
            // change, database wiped). The user still has a valid TOTP secret, so instead
            // of tearing down the connection we re-open the 2FA stage and ask for a code.
            // |token_rejected| tells the client to drop the stale local copy and not retry
            // the token path on this challenge.
            CLOG(INFO) << "Device token rejected for user" << userName() << "- asking for TOTP";

            proto::router::RouterToClient message;
            proto::router::TwoFactorChallenge* challenge = message.mutable_two_factor_challenge();
            challenge->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
            challenge->set_token_rejected(true);
            sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
            return;
        }

        Database::instance().touchClientDeviceToken(token, address());
        token_id_ = token_id;
        completeTwoFactor();
        return;
    }

    const QByteArray& secret = enroll ? tentative_otp_secret_ : user_otp_secret_;

    if (secret.isEmpty() || response.totp_code().empty())
    {
        CLOG(INFO) << "Empty TOTP code or no OTP secret for user" << userName() << ". Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    const QString code = QString::fromStdString(response.totp_code());
    quint64 matched_counter = 0;
    if (!Totp::verify(secret, code, now, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                      Totp::kDefaultWindowSteps, &matched_counter))
    {
        CLOG(INFO) << "Invalid TOTP code for user" << userName() << ". Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    // Replay protection: refuse any code whose step has already been consumed. ENROLL starts
    // from counter 0, so the first valid code (any positive step) is accepted.
    if (!enroll && matched_counter <= user_otp_counter_)
    {
        CLOG(INFO) << "Replayed TOTP code for user" << userName() << ". Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    if (enroll)
    {
        if (!Database::instance().setUserOtp(userId(), tentative_otp_secret_, matched_counter))
        {
            CLOG(ERROR) << "Failed to persist OTP secret for user" << userName()
                        << ". Closing connection";
            emit sig_finished(session_id_);
            return;
        }
        tentative_otp_secret_.clear();
    }
    else
    {
        if (!Database::instance().consumeUserOtpCounter(userId(), matched_counter))
        {
            CLOG(INFO) << "TOTP counter was already consumed for user" << userName()
                       << ". Closing connection";
            emit sig_finished(session_id_);
            return;
        }
    }

    // Any successful TOTP submission produces a fresh bearer token. Failure to persist the
    // token is non-fatal: the user is still let in, they will be prompted for TOTP again
    // next time.
    std::string new_token;
    qint64 new_token_id = 0;
    if (!Database::instance().issueClientDeviceToken(userId(), address(), &new_token, &new_token_id))
        CLOG(WARNING) << "Failed to issue device token for user" << userName();

    token_id_ = new_token_id;

    completeTwoFactor(std::move(new_token));
}

//--------------------------------------------------------------------------------------------------
void Client::completeTwoFactor(std::string&& new_token)
{
    // 2FA passed. A TwoFactorResult is sent only to hand over a freshly issued device token;
    // the token-path success carries no token, and an empty message cannot be sent over the
    // wire, so in that case success is signalled by UserKeys alone.
    if (!new_token.empty())
    {
        proto::router::RouterToClient envelope;
        envelope.mutable_two_factor_result()->set_new_token(std::move(new_token));
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(envelope));
    }

    two_factor_completed_ = true;
    sendUserKeys();
}

//--------------------------------------------------------------------------------------------------
void Client::sendUserKeys()
{
    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    RouterUser user = database.findUser(userId());
    if (!user.isValid())
    {
        CLOG(WARNING) << "Authenticated user not found in database (user_id:" << userId() << ")";
        return;
    }

    proto::router::RouterToClient message;
    proto::router::UserKeys* user_keys = message.mutable_user_keys();
    user_keys->set_user_id(user.entry_id);
    user_keys->set_name(user.name.toStdString());
    user_keys->set_public_key(user.public_key.toStdString());
    user_keys->set_wrap_private_key(user.wrap_private_key.toStdString());
    user_keys->set_wrap_salt(user.wrap_salt.toStdString());

    std::vector<Workspace::Access> keys = database.workspaceAccessListForUser(user.entry_id);
    for (Workspace::Access& access : keys)
    {
        proto::router::UserKeys::WorkspaceKey* dst = user_keys->add_workspace_key();
        dst->set_workspace_id(access.workspace_id);
        dst->set_wrapped_gk(std::move(access.wrapped_gk));
    }

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    CLOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";

    const HostId host_id = request.host_id();

    proto::router::RouterToClient message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();
    offer->set_request_id(request.request_id());

    std::optional<SharedHosts::Host> host_info = SharedHosts::instance().find(host_id);
    if (!host_info.has_value())
    {
        CLOG(ERROR) << "Host with id" << host_id << "NOT found!";
        offer->set_error_code(proto::router::ConnectionOffer::PEER_NOT_FOUND);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    CLOG(INFO) << "Host with id" << host_id << "found";

    std::optional<SharedKeyPool::Credentials> credentials = SharedKeyPool::instance().take();
    if (!credentials.has_value())
    {
        CLOG(ERROR) << "Empty key pool";
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);
    relay_worker->notifyKeyUsed(credentials->session_id, credentials->key.key_id());

    offer->set_error_code(proto::router::ConnectionOffer::SUCCESS);

    proto::router::PeerInfo* peer_info = offer->mutable_peer_info();
    peer_info->set_is_legacy(host_info->version < kVersion_3_0_0);

    if (stun_port_)
    {
        // An empty host string means that the client should use the router's address
        // as the server's host address. This is done to allow for future expansion,
        // but is not currently used.
        proto::router::StunServerInfo* stun_info = offer->mutable_stun_info();
        stun_info->set_version(1);
        stun_info->set_host("");
        stun_info->set_port(stun_port_);
    }

    proto::router::RelayCredentials* offer_credentials = offer->mutable_relay();
    offer_credentials->set_host(credentials->peer_host);
    offer_credentials->set_port(credentials->peer_port);
    offer_credentials->mutable_key()->Swap(&credentials->key);

    // AES-256-GCM is used only when both peers support it; legacy peers (and their relay code) only
    // understand ChaCha20-Poly1305. The key material itself is algorithm-agnostic.
    const bool aes = host_info->version >= kVersion_3_0_0 && version() >= kVersion_3_0_0;
    offer_credentials->mutable_key()->set_encryption(aes ?
        proto::router::RelayKey::ENCRYPTION_AES256_GCM : proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305);

    proto::relay::PeerToRelay::Secret secret;
    secret.set_random_data(Random::string(16));
    secret.set_client_address(address());
    secret.set_client_user_name(userName());
    secret.set_host_address(host_info->address);
    secret.set_host_id(host_id);

    offer_credentials->set_secret(secret.SerializeAsString());

    // The host could disconnect before the offer reaches its worker; the offer is then dropped
    // there and the consumed one-time key is lost, which is acceptable - relays replenish the
    // pool continuously.
    HostWorker* host_worker = CoreApplication::findWorker<HostWorker>();
    CHECK(host_worker);

    CLOG(INFO) << "Sending connection offer to host";
    host_worker->sendConnectionOffer(host_id, *offer);

    CLOG(INFO) << "Sending connection offer to client";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToClient message;
    proto::router::HostStatus* host_status = message.mutable_host_status();
    host_status->set_request_id(check_host_status.request_id());

    std::optional<SharedHosts::Host> host_info = SharedHosts::instance().find(check_host_status.host_id());
    if (host_info.has_value())
    {
        host_status->set_status(proto::router::HostStatus::STATUS_ONLINE);
        host_status->mutable_version()->CopyFrom(serialize(host_info->version));
    }
    else
    {
        host_status->set_status(proto::router::HostStatus::STATUS_OFFLINE);
    }

    CLOG(INFO) << "Sending host status:" << *host_status;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readHostListRequest(const proto::router::HostListRequest& request)
{
    const proto::router::HostListRequest::Mode mode = request.mode();
    const qint64 workspace_id = request.workspace_id();
    const qint64 group_id = request.group_id();
    const qint64 start_item = request.start_item();
    const qint64 end_item = request.end_item();
    const bool is_admin = sessionType() == proto::router::SESSION_TYPE_ADMIN;

    proto::router::RouterToClient message;
    proto::router::HostList* result = message.mutable_host_list();
    result->set_request_id(request.request_id());
    result->set_workspace_id(workspace_id);
    result->set_group_id(group_id);

    if (mode != proto::router::HostListRequest::MODE_ALL &&
        mode != proto::router::HostListRequest::MODE_FILTERED)
    {
        CLOG(ERROR) << "Unknown host list mode:" << mode;
        result->set_error_code(proto::router::kErrorInvalidRequest);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    if (mode == proto::router::HostListRequest::MODE_ALL && !is_admin)
    {
        CLOG(ERROR) << "Non-admin requested MODE_ALL host list";
        result->set_error_code(proto::router::kErrorAccessDenied);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    if (mode == proto::router::HostListRequest::MODE_FILTERED &&
        !database.hasWorkspaceAccess(userId(), workspace_id))
    {
        CLOG(ERROR) << "User" << userId() << "has no access to workspace" << workspace_id;
        result->set_error_code(proto::router::kErrorAccessDenied);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    if (mode == proto::router::HostListRequest::MODE_ALL)
    {
        result->set_total_count(database.hostCount());
        database.hosts(start_item, end_item, result);
    }
    else
    {
        result->set_total_count(database.hostCount(workspace_id, group_id));
        database.hosts(workspace_id, group_id, start_item, end_item, result);
    }

    // Mark currently connected hosts as online.
    for (proto::router::Host& host : *result->mutable_host())
        host.set_online(SharedHosts::instance().contains(host.host_id()));

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readHostSearchRequest(const proto::router::HostSearchRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::HostSearchResult* result = message.mutable_host_search_result();
    result->set_request_id(request.request_id());

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    // Search is always scoped to every workspace the user can access, regardless of session type.
    // Only the ids are needed here, so avoid pulling each membership's wrapped_gk blob.
    const std::set<qint64> workspace_ids = database.workspaceAccessIdsForUser(userId());

    database.searchHosts(QString::fromStdString(request.query()), workspace_ids, result);

    // Mark currently connected hosts as online.
    for (proto::router::Host& host : *result->mutable_host())
        host.set_online(SharedHosts::instance().contains(host.host_id()));

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readTempHostListRequest(const proto::router::TempHostListRequest& request)
{
    HostWorker* host_worker = CoreApplication::findWorker<HostWorker>();
    CHECK(host_worker);

    const bool is_admin = sessionType() == proto::router::SESSION_TYPE_ADMIN;
    const auto request_id = request.request_id();

    host_worker->requestTempHostList(is_admin, this,
        [this, request_id](proto::router::TempHostList&& temp_host_list)
    {
        temp_host_list.set_request_id(request_id);
        temp_host_list.set_error_code(proto::router::kErrorOk);

        proto::router::RouterToClient message;
        message.mutable_temp_host_list()->Swap(&temp_host_list);

        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
    });
}

//--------------------------------------------------------------------------------------------------
void Client::readWorkspaceListRequest(const proto::router::WorkspaceListRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::WorkspaceList* list = message.mutable_workspace_list();
    list->set_request_id(request.request_id());

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        list->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    // Each session sees only the workspaces it has a workspace_access entry for. Admins get the
    // full access list per workspace (needed to manage membership); other sessions get only their
    // own entry. workspace_id == 0 means all visible workspaces; > 0 narrows to a single entry.
    if (sessionType() == proto::router::SESSION_TYPE_ADMIN)
        database.workspaceListWithAllAccess(userId(), request.workspace_id(), list);
    else
        database.workspaceListWithOwnAccess(userId(), request.workspace_id(), list);

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readGroupListRequest(const proto::router::GroupListRequest& request)
{
    const qint64 workspace_id = request.workspace_id();

    proto::router::RouterToClient message;
    proto::router::GroupList* result = message.mutable_group_list();
    result->set_request_id(request.request_id());
    result->set_workspace_id(workspace_id);

    if (workspace_id <= 0)
    {
        CLOG(ERROR) << "Invalid workspace id in group list request:" << workspace_id;
        result->set_error_code(proto::router::kErrorInvalidRequest);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    if (!database.hasWorkspaceAccess(userId(), workspace_id))
    {
        CLOG(ERROR) << "User" << userId() << "has no access to workspace" << workspace_id;
        result->set_error_code(proto::router::kErrorAccessDenied);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    database.groupList(workspace_id, result);

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readChangePasswordRequest(const proto::router::ChangePasswordRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::ChangePasswordResult* result = message.mutable_change_password_result();
    result->set_request_id(request.request_id());

    Database& database = Database::instance();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    RouterUser user = database.findUser(userId());
    if (!user.isValid())
    {
        CLOG(WARNING) << "Authenticated user not found in database (user_id:" << userId() << ")";
        result->set_error_code(proto::router::kErrorInvalidData);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    // Replace only the password-derived fields; keep name, group, sessions, flags intact.
    user.salt             = QByteArray::fromStdString(request.salt());
    user.verifier         = QByteArray::fromStdString(request.verifier());
    user.public_key       = QByteArray::fromStdString(request.public_key());
    user.wrap_private_key = QByteArray::fromStdString(request.wrap_private_key());
    user.wrap_salt        = QByteArray::fromStdString(request.wrap_salt());

    if (!user.isValid())
    {
        CLOG(ERROR) << "Rotated credentials produced an invalid user record";
        result->set_error_code(proto::router::kErrorInvalidData);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    // The rotation produced a new key pair, so the workspace keys re-sealed by the client to the
    // new public key must replace the stored ones (now sealed to the old, discarded key).
    std::unordered_map<qint64, QByteArray> wrapped_keys;
    wrapped_keys.reserve(request.workspace_key_size());

    for (int i = 0; i < request.workspace_key_size(); ++i)
    {
        const proto::router::ChangePasswordRequest::WorkspaceKey& wk = request.workspace_key(i);
        wrapped_keys.emplace(wk.workspace_id(), QByteArray::fromStdString(wk.wrapped_gk()));
    }

    // Credentials and re-wrapped keys are persisted atomically: the password is rotated only if a
    // re-sealed key is present for every workspace the user can access, so a partial set can never
    // leave the user without workspace access. Only the password-derived fields differ here (the
    // rest were loaded from the database), so reusing modifyUser writes back identical values.
    const std::string_view error_code = database.modifyUser(user, wrapped_keys);
    if (error_code != proto::router::kErrorOk)
    {
        CLOG(ERROR) << "Failed to change password for user" << userName() << ":" << error_code;
        result->set_error_code(error_code);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    CLOG(INFO) << "User" << userName() << "rotated own credentials";
    result->set_error_code(proto::router::kErrorOk);
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));

    emit sig_notifyChanged(ClientWorker::NOTIFY_USERS);

    // This request always rotates the password (tokens are revoked in the transaction). Drop the
    // user's other live sessions but keep this one.
    emit sig_stopClients(userId(), {}, sessionId());

    // The rotation revoked every device token, including this session's. Re-run the 2FA stage
    // exactly as on a fresh connection: clear the completion flag and re-challenge. The client
    // must pass 2FA again before it gets the new UserKeys (sent by completeTwoFactor); a failed
    // attempt tears the session down inside readTwoFactorResponse.
    two_factor_completed_ = false;
    token_id_ = 0;
    doTwoFactorChallenge();
}

