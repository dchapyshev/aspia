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
#include <QSet>

#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/crypto/totp.h"
#include "proto/relay_peer.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_legacy_host.h"
#include "router/database.h"
#include "router/relay.h"
#include "router/service.h"
#include "router/host.h"
#include "router/host_ng.h"
#include "router/host_legacy.h"

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
    address_.setAddress(tcp_channel_->peerAddress());

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Client::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Client::onTcpMessageReceived);
    connect(this, &Client::sig_started, this, &Client::onStarted);

    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Client::~Client()
{
    CLOG(INFO) << "Dtor";
    Service::instance()->notifyChanged(Service::NOTIFY_CLIENTS);
}

//--------------------------------------------------------------------------------------------------
void Client::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);

    Service::instance()->notifyChanged(Service::NOTIFY_CLIENTS);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Client::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Client::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Client::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Client::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Client::userName() const
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
std::chrono::seconds Client::duration() const
{
    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::from_time_t(start_time_);

    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - time_point);
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
        CLOG(WARNING) << "Authenticated user disappeared from database (user_id:" << userId() << ")";
        sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INTERNAL_ERROR);
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
        const QString uri = Totp::buildUri(kOtpIssuer, userName(), tentative_otp_secret_);

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
        const QByteArray token = QByteArray::fromStdString(response.token());

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
            CLOG(INFO) << "Device token rejected for user" << userId() << "- asking for TOTP";

            proto::router::RouterToClient message;
            proto::router::TwoFactorChallenge* challenge = message.mutable_two_factor_challenge();
            challenge->set_mode(proto::router::TWO_FACTOR_MODE_ACTIVE);
            challenge->set_token_rejected(true);
            sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
            return;
        }

        Database::instance().touchClientDeviceToken(token, address().toString());
        token_id_ = token_id;
        sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_OK);
        return;
    }

    const QByteArray& secret = enroll ? tentative_otp_secret_ : user_otp_secret_;

    if (secret.isEmpty() || response.totp_code().empty())
    {
        sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INVALID_CODE);
        return;
    }

    const QString code = QString::fromStdString(response.totp_code());
    quint64 matched_counter = 0;
    if (!Totp::verify(secret, code, now, Totp::kDefaultStepSec, Totp::kDefaultDigits,
                      Totp::kDefaultWindowSteps, &matched_counter))
    {
        sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INVALID_CODE);
        return;
    }

    // Replay protection: refuse any code whose step has already been consumed. ENROLL starts
    // from counter 0, so the first valid code (any positive step) is accepted.
    if (!enroll && matched_counter <= user_otp_counter_)
    {
        sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INVALID_CODE);
        return;
    }

    if (enroll)
    {
        if (!Database::instance().setUserOtp(userId(), tentative_otp_secret_, matched_counter))
        {
            CLOG(ERROR) << "Failed to persist OTP secret for user" << userId();
            sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INTERNAL_ERROR);
            return;
        }
        tentative_otp_secret_.clear();
    }
    else
    {
        if (!Database::instance().updateUserOtpCounter(userId(), matched_counter))
        {
            CLOG(ERROR) << "Failed to update OTP counter for user" << userId();
            sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_INTERNAL_ERROR);
            return;
        }
    }

    // Any successful TOTP submission produces a fresh bearer token. Failure to persist the
    // token is non-fatal: the user is still let in, they will be prompted for TOTP again
    // next time.
    QByteArray new_token;
    qint64 new_token_id = 0;
    if (!Database::instance().issueClientDeviceToken(
            userId(), address().toString(), &new_token, &new_token_id))
    {
        CLOG(WARNING) << "Failed to issue device token for user" << userId();
        new_token = QByteArray();
    }
    token_id_ = new_token_id;

    sendTwoFactorResult(proto::router::TWO_FACTOR_STATUS_OK, new_token);
}

//--------------------------------------------------------------------------------------------------
void Client::sendTwoFactorResult(proto::router::TwoFactorStatus status, const QByteArray& new_token)
{
    proto::router::RouterToClient envelope;
    proto::router::TwoFactorResult* result = envelope.mutable_two_factor_result();
    result->set_status(status);
    if (!new_token.isEmpty())
        result->set_new_token(new_token.toStdString());

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(envelope));

    if (status == proto::router::TWO_FACTOR_STATUS_OK)
    {
        two_factor_completed_ = true;
        sendUserKeys();
    }
    // On failure the channel is dropped when the client disconnects in response; we
    // intentionally do not call any disconnect API here because the result message must
    // reach the client first.
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

    const QList<Workspace::Access> keys = database.workspaceAccessListForUser(user.entry_id);
    for (const Workspace::Access& access : std::as_const(keys))
    {
        proto::router::UserKeys::WorkspaceKey* dst = user_keys->add_workspace_key();
        dst->set_workspace_id(access.workspace_id);
        dst->set_wrapped_gk(access.wrapped_gk.toStdString());
    }

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    CLOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";

    proto::router::RouterToClient message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();
    offer->set_request_id(request.request_id());

    Host* host = hostByHostId(request.host_id());
    if (!host)
    {
        CLOG(ERROR) << "Host with id" << request.host_id() << "NOT found!";
        offer->set_error_code(proto::router::ConnectionOffer::PEER_NOT_FOUND);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    CLOG(INFO) << "Host with id" << request.host_id() << "found";

    std::optional<Service::Credentials> credentials = Service::instance()->takeCredentials();
    if (!credentials.has_value())
    {
        CLOG(ERROR) << "Empty key pool";
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    Relay* relay = Service::instance()->relay(credentials->session_id);
    if (!relay)
    {
        CLOG(ERROR) << "No relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    const std::optional<Relay::PeerData>& peer_data = relay->peerData();
    if (!peer_data.has_value())
    {
        CLOG(ERROR) << "No peer data for relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    offer->set_error_code(proto::router::ConnectionOffer::SUCCESS);

    proto::router::PeerInfo* peer_info = offer->mutable_peer_info();
    peer_info->set_is_legacy(host->version() < kVersion_3_0_0);
    peer_info->set_is_address_equals(host->address() == address());

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
    offer_credentials->set_host(relay->peerData()->first);
    offer_credentials->set_port(relay->peerData()->second);
    offer_credentials->mutable_key()->Swap(&credentials->key);

    proto::relay::PeerToRelay::Secret secret;
    secret.set_random_data(Random::string(16));
    secret.set_client_address(address().toString().toStdString());
    secret.set_client_user_name(userName().toStdString());
    secret.set_host_address(host->address().toString().toStdString());
    secret.set_host_id(request.host_id());

    offer_credentials->set_secret(secret.SerializeAsString());

    CLOG(INFO) << "Sending connection offer to host";
    HostNG* host_ng = dynamic_cast<HostNG*>(host);
    if (host_ng)
    {
        host_ng->sendConnectionOffer(*offer);
    }
    else
    {
        HostLegacy* host_legacy = static_cast<HostLegacy*>(host);

        proto::router::legacy::ConnectionOffer legacy_offer;
        legacy_offer.set_peer_role(proto::router::legacy::ConnectionOffer::HOST);
        legacy_offer.set_error_code(proto::router::legacy::ConnectionOffer::SUCCESS);
        legacy_offer.mutable_relay()->CopyFrom(offer->relay());
        legacy_offer.mutable_host_data()->set_host_id(request.host_id());

        host_legacy->sendConnectionOffer(legacy_offer);
    }

    CLOG(INFO) << "Sending connection offer to client";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Client::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToClient message;
    proto::router::HostStatus* host_status = message.mutable_host_status();
    host_status->set_request_id(check_host_status.request_id());

    Host* host = hostByHostId(check_host_status.host_id());
    if (host)
    {
        host_status->set_status(proto::router::HostStatus::STATUS_ONLINE);
        host_status->mutable_version()->CopyFrom(serialize(host->version()));
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

    // Collect host_ids of currently connected hosts to mark them online.
    QSet<HostId> online_host_ids;
    const QList<Host*>& online_hosts = Service::instance()->hosts();
    for (Host* host : std::as_const(online_hosts))
    {
        HostNG* host_ng = dynamic_cast<HostNG*>(host);
        if (host_ng)
        {
            online_host_ids.insert(host_ng->hostId());
            continue;
        }

        HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host);
        if (host_legacy)
        {
            for (HostId host_id : host_legacy->hostIdList())
                online_host_ids.insert(host_id);
        }
    }

    QList<HostInfo> hosts;
    qint64 hosts_count = 0;

    if (mode == proto::router::HostListRequest::MODE_ALL)
    {
        hosts_count = database.hostCount();
        hosts = database.hosts(start_item, end_item);
    }
    else
    {
        hosts_count = database.hostCount(workspace_id, group_id);
        hosts = database.hosts(workspace_id, group_id, start_item, end_item);
    }

    result->set_error_code(proto::router::kErrorOk);
    result->set_total_count(hosts_count);

    for (const HostInfo& info : std::as_const(hosts))
    {
        proto::router::Host* item = result->add_host();
        item->set_host_id(info.host_id);
        item->set_workspace_id(info.workspace_id);
        item->set_group_id(info.group_id);
        item->set_display_name(info.display_name.toStdString());
        item->set_computer_name(info.computer_name.toStdString());
        item->set_cpu_arch(info.cpu_arch.toStdString());
        item->set_version(info.version.toStdString());
        item->set_os_name(info.os_name.toStdString());
        item->set_address(info.address.toStdString());
        item->set_comment(info.comment.toStdString());
        item->set_user_name(info.user_name.toStdString());
        item->set_password(info.password.toStdString());
        item->set_last_connect(info.last_connect);
        item->set_last_modify(info.last_modify);
        item->set_online(online_host_ids.contains(info.host_id));
    }

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
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

    list->set_error_code(proto::router::kErrorOk);

    // Each session sees only the workspaces it has a workspace_access entry for. Admin gets
    // the full access list for each (needed to manage membership); other session types get
    // only their own entry (only their own wrapped_gk is needed to decrypt the workspace GK).
    const bool is_admin = sessionType() == proto::router::SESSION_TYPE_ADMIN;
    const QSet<qint64> accessible_ids = database.workspaceAccessIdsForUser(userId());
    const QList<Workspace> workspaces = database.workspaceList();

    // workspace_id == 0 means "all visible workspaces"; > 0 narrows to a single entry.
    const qint64 filter_id = request.workspace_id();

    for (const Workspace& workspace : std::as_const(workspaces))
    {
        if (filter_id > 0 && workspace.entry_id != filter_id)
            continue;
        if (!accessible_ids.contains(workspace.entry_id))
            continue;

        proto::router::Workspace* item = list->add_workspace();
        item->set_entry_id(workspace.entry_id);
        item->set_name(workspace.name.toStdString());
        item->set_comment(workspace.comment.toStdString());

        const QList<Workspace::Access> accesses = database.workspaceAccessList(workspace.entry_id);
        for (const Workspace::Access& access : std::as_const(accesses))
        {
            if (!is_admin && access.user_id != userId())
                continue;

            proto::router::WorkspaceAccess* access_item = item->add_access();
            access_item->set_user_id(access.user_id);
            access_item->set_wrapped_gk(access.wrapped_gk.toStdString());
        }
    }

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

    const QList<Group> groups = database.groupList(workspace_id);

    result->set_error_code(proto::router::kErrorOk);
    for (const Group& group : std::as_const(groups))
    {
        proto::router::Group* item = result->add_group();
        item->set_entry_id(group.entry_id);
        item->set_parent_id(group.parent_id);
        item->set_name(group.name.toStdString());
        item->set_comment(group.comment.toStdString());
    }

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

    if (!database.modifyUser(user))
    {
        CLOG(ERROR) << "Failed to persist rotated user credentials";
        result->set_error_code(proto::router::kErrorInternalError);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    CLOG(INFO) << "User" << userName() << "rotated own credentials";
    result->set_error_code(proto::router::kErrorOk);
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));

    // This request always rotates the password (tokens are revoked in the transaction). Drop the
    // user's other live sessions but keep this one, which re-keys below.
    Service::instance()->stopClients(userId(), {}, sessionId());

    // Push a fresh UserKeys so the client can decrypt with the new wrap key and transition
    // from CONNECTING to ONLINE.
    sendUserKeys();
}

//--------------------------------------------------------------------------------------------------
Host* Client::hostByHostId(HostId host_id)
{
    const QList<Host*>& hosts = Service::instance()->hosts();

    for (const auto& host : hosts)
    {
        HostNG* host_ng = dynamic_cast<HostNG*>(host);
        if (host_ng && host_ng->hostId() == host_id)
            return host_ng;

        HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host);
        if (host_legacy && host_legacy->hasHostId(host_id))
            return host_legacy;
    }

    return nullptr;
}
