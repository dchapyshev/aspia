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

#include "router/session_client.h"

#include <QSet>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "proto/relay_peer.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_legacy_host.h"
#include "router/database.h"
#include "router/service.h"
#include "router/session_host.h"
#include "router/session_legacy_host.h"
#include "router/session_relay.h"

//--------------------------------------------------------------------------------------------------
SessionClient::SessionClient(TcpChannel* channel, QObject* parent)
    : Session(channel, parent)
{
    CLOG(INFO) << "Ctor";
    connect(this, &Session::sig_started, this, &SessionClient::onStarted);
}

//--------------------------------------------------------------------------------------------------
SessionClient::~SessionClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SessionClient::setStunInfo(quint16 port)
{
    stun_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::router::CHANNEL_ID_CLIENT)
        return;

    proto::router::ClientToRouter message;
    if (!parse(buffer, &message))
    {
        CLOG(ERROR) << "Could not read message from client";
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
        readWorkspaceListRequest();
    }
    else
    {
        CLOG(ERROR) << "Unhandled message from client";
    }
}

//--------------------------------------------------------------------------------------------------
void SessionClient::onStarted()
{
    Database database = Database::open();
    if (!database.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database";
        return;
    }

    RouterUser user = database.findUser(userName());
    if (!user.isValid())
    {
        CLOG(WARNING) << "Authenticated user not found in database:" << userName();
        return;
    }

    proto::router::RouterToClient message;
    proto::router::UserKeys* user_keys = message.mutable_user_keys();
    user_keys->set_user_id(user.entry_id);
    user_keys->set_name(user.name.toStdString());
    user_keys->set_public_key(user.public_key.toStdString());
    user_keys->set_wrap_private_key(user.wrap_private_key.toStdString());
    user_keys->set_wrap_salt(user.wrap_salt.toStdString());

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    CLOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";

    proto::router::RouterToClient message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();
    offer->set_request_id(request.request_id());

    Session* session = sessionByHostId(request.host_id());
    if (!session)
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

    SessionRelay* relay = static_cast<SessionRelay*>(Service::instance()->session(credentials->session_id));
    if (!relay)
    {
        CLOG(ERROR) << "No relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    const std::optional<SessionRelay::PeerData>& peer_data = relay->peerData();
    if (!peer_data.has_value())
    {
        CLOG(ERROR) << "No peer data for relay with session id" << credentials->session_id;
        offer->set_error_code(proto::router::ConnectionOffer::KEY_POOL_EMPTY);
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    offer->set_error_code(proto::router::ConnectionOffer::SUCCESS);

    proto::router::PeerInfo* peer_info = offer->mutable_peer_info();
    peer_info->set_is_legacy(session->version() < kVersion_3_0_0);
    peer_info->set_is_address_equals(session->address() == address());

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
    secret.set_host_address(session->address().toString().toStdString());
    secret.set_host_id(request.host_id());

    offer_credentials->set_secret(secret.SerializeAsString());

    CLOG(INFO) << "Sending connection offer to host";
    SessionHost* host_session = dynamic_cast<SessionHost*>(session);
    if (host_session)
    {
        host_session->sendConnectionOffer(*offer);
    }
    else
    {
        SessionLegacyHost* legacy_host_session = static_cast<SessionLegacyHost*>(session);

        proto::router::legacy::ConnectionOffer legacy_offer;
        legacy_offer.set_peer_role(proto::router::legacy::ConnectionOffer::HOST);
        legacy_offer.set_error_code(proto::router::legacy::ConnectionOffer::SUCCESS);
        legacy_offer.mutable_relay()->CopyFrom(offer->relay());
        legacy_offer.mutable_host_data()->set_host_id(request.host_id());

        legacy_host_session->sendConnectionOffer(legacy_offer);
    }

    CLOG(INFO) << "Sending connection offer to client";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToClient message;
    proto::router::HostStatus* host_status = message.mutable_host_status();
    host_status->set_request_id(check_host_status.request_id());

    Session* session = sessionByHostId(check_host_status.host_id());
    if (session)
    {
        host_status->set_status(proto::router::HostStatus::STATUS_ONLINE);
        host_status->mutable_version()->CopyFrom(serialize(session->version()));
    }
    else
    {
        host_status->set_status(proto::router::HostStatus::STATUS_OFFLINE);
    }

    CLOG(INFO) << "Sending host status:" << *host_status;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void SessionClient::readHostListRequest(const proto::router::HostListRequest& request)
{
    const proto::router::HostListRequest::Mode mode = request.mode();
    const qint64 workspace_id = request.workspace_id();
    const qint64 group_id = request.group_id();
    const qint64 start_item = request.start_item();
    const qint64 end_item = request.end_item();
    const bool is_admin = sessionType() == proto::router::SESSION_TYPE_ADMIN;

    proto::router::RouterToClient message;
    proto::router::HostList* result = message.mutable_host_list();
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

    Database database = Database::open();
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
    QSet<qint64> online_host_ids;
    const QList<Session*>& sessions = Service::instance()->sessions();
    for (Session* session : std::as_const(sessions))
    {
        if (session->sessionType() != proto::router::SESSION_TYPE_HOST)
            continue;

        SessionHost* host_session = dynamic_cast<SessionHost*>(session);
        if (host_session)
        {
            online_host_ids.insert(static_cast<qint64>(host_session->hostId()));
            continue;
        }

        SessionLegacyHost* legacy_host_session = dynamic_cast<SessionLegacyHost*>(session);
        if (legacy_host_session)
        {
            for (HostId host_id : legacy_host_session->hostIdList())
                online_host_ids.insert(static_cast<qint64>(host_id));
        }
    }

    const QVector<HostInfo> hosts = mode == proto::router::HostListRequest::MODE_ALL ?
        database.hosts(start_item, end_item) : database.hosts(workspace_id, group_id, start_item, end_item);
    result->set_error_code(proto::router::kErrorOk);

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
void SessionClient::readWorkspaceListRequest()
{
    proto::router::RouterToClient message;
    proto::router::WorkspaceList* list = message.mutable_workspace_list();

    Database database = Database::open();
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
    const QSet<qint64> accessible_ids = database.workspaceAccessListForUser(userId());
    const QVector<Workspace> workspaces = database.workspaceList();

    for (const Workspace& workspace : std::as_const(workspaces))
    {
        if (!accessible_ids.contains(workspace.entry_id))
            continue;

        proto::router::Workspace* item = list->add_workspace();
        item->set_entry_id(workspace.entry_id);
        item->set_name(workspace.name.toStdString());
        item->set_comment(workspace.comment.toStdString());

        const QVector<Workspace::Access> accesses = database.workspaceAccessList(workspace.entry_id);
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
Session* SessionClient::sessionByHostId(HostId host_id)
{
    QList<Session*> session_list = Service::instance()->sessions();

    for (const auto& session : std::as_const(session_list))
    {
        if (session->sessionType() == proto::router::SESSION_TYPE_HOST)
        {
            SessionHost* host_session = dynamic_cast<SessionHost*>(session);
            if (host_session && host_session->hostId() == host_id)
                return host_session;

            SessionLegacyHost* legacy_host_session = dynamic_cast<SessionLegacyHost*>(session);
            if (legacy_host_session && legacy_host_session->hasHostId(host_id))
                return legacy_host_session;
        }
    }

    return nullptr;
}
