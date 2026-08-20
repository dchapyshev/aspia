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

#include "router/client_operator.h"

#include <QDateTime>

#include "base/core_application.h"
#include "base/serialization.h"
#include "base/threading/worker.h"
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/shared_hosts.h"
#include "router/shared_key_pool.h"
#include "router/handlers/connection_request_handler.h"
#include "router/handlers/group_request_handler.h"
#include "router/handlers/host_request_handler.h"
#include "router/handlers/user_request_handler.h"
#include "router/handlers/workspace_request_handler.h"
#include "router/workers/client_worker.h"
#include "router/workers/host_worker.h"
#include "router/workers/relay_worker.h"

namespace {

//--------------------------------------------------------------------------------------------------
qint64 createClientId()
{
    static qint64 last_client_id = 0;
    ++last_client_id;
    return last_client_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientOperator::ClientOperator(Database& database, TcpChannel* channel, QObject* parent)
    : QObject(parent),
      database_(database),
      session_id_(createClientId()),
      tcp_channel_(channel)
{
    CDCHECK(tcp_channel_);
    tcp_channel_->setParent(this);

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &ClientOperator::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &ClientOperator::onTcpMessageReceived);
    connect(this, &ClientOperator::sig_started, this, &ClientOperator::onStarted);
    connect(Worker::current(), &Worker::sig_tick, tcp_channel_, &TcpChannel::tick);

    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientOperator::~ClientOperator()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);
    emit sig_notifyChanged(ClientWorker::NOTIFY_CLIENTS);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber ClientOperator::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
const std::string& ClientOperator::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
const std::string& ClientOperator::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
const std::string& ClientOperator::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
const std::string& ClientOperator::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
qint64 ClientOperator::userId() const
{
    return tcp_channel_->peerUserId();
}

//--------------------------------------------------------------------------------------------------
proto::router::SessionType ClientOperator::sessionType() const
{
    return static_cast<proto::router::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::sendMessage(quint8 channel_id, const QByteArray& message)
{
    tcp_channel_->send(channel_id, message);
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::setStunInfo(quint16 port)
{
    stun_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::onSessionMessage(quint8 channel_id, const QByteArray& buffer)
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
RequestCaller ClientOperator::requestCaller() const
{
    RequestCaller caller;
    caller.user_id = userId();
    caller.name = userName();
    caller.session_type = sessionType();
    return caller;
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::applyRequestResult(const RequestResult& result)
{
    if (result.notify_flags)
        emit sig_notifyChanged(result.notify_flags);

    if (result.stop_user_id > 0)
        emit sig_stopClients(result.stop_user_id, result.stop_token_ids, 0);
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    CLOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    onSessionMessage(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::onStarted()
{
    doTwoFactorChallenge();
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::doTwoFactorChallenge()
{
    applyTwoFactorResult(two_factor_.start(database_, requestCaller()));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readTwoFactorResponse(const proto::router::TwoFactorResponse& response)
{
    applyTwoFactorResult(two_factor_.handleResponse(
        database_, requestCaller(), response, address(),
        QDateTime::currentSecsSinceEpoch()));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::applyTwoFactorResult(TwoFactorHandler::Result&& result)
{
    switch (result.action)
    {
        case TwoFactorHandler::Action::SEND_CHALLENGE:
        {
            proto::router::RouterToClient message;
            proto::router::TwoFactorChallenge* challenge = message.mutable_two_factor_challenge();
            challenge->set_mode(result.challenge.mode);
            if (!result.challenge.otpauth_uri.empty())
                challenge->set_otpauth_uri(std::move(result.challenge.otpauth_uri));
            if (result.challenge.token_rejected)
                challenge->set_token_rejected(true);

            sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        }
        return;

        case TwoFactorHandler::Action::ACCEPT:
            token_id_ = result.token_id;
            completeTwoFactor(std::move(result.new_token));
            return;

        case TwoFactorHandler::Action::CLOSE:
            emit sig_finished(session_id_);
            return;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::completeTwoFactor(std::string&& new_token)
{
    // The account is read once more here: the session was authenticated against the user list, and
    // a user removed in between must not get in. Either failure tears the session down, because a
    // client that never receives LoginResult hangs in the connecting state with no error and
    // nothing retries the send.
    if (!database_.isValid())
    {
        CLOG(ERROR) << "Failed to connect to database. Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    RouterUser user;
    if (database_.findUser(userId(), &user) != proto::router::kErrorOk)
    {
        CLOG(WARNING) << "Authenticated user not found in database (user_id:" << userId()
                      << "). Closing connection";
        emit sig_finished(session_id_);
        return;
    }

    two_factor_completed_ = true;

    // Sent on every login, with or without a token to hand over: this is what opens the session on
    // the client side.
    proto::router::RouterToClient message;
    message.mutable_login_result()->set_new_token(std::move(new_token));
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readConnectionRequest(const proto::router::ConnectionRequest& request)
{
    CLOG(INFO) << "New connection request (host_id:" << request.host_id() << ")";
    sendConnectionOffer(request.request_id(), request.host_id());
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::sendConnectionOffer(qint64 request_id, HostId host_id)
{
    ConnectionRequestClient client;
    client.host_id = host_id;
    client.version = version();
    client.address = address();
    client.user_name = userName();
    client.stun_port = stun_port_;

    ConnectionRequestResult built = handleConnectionRequest(
        SharedHosts::instance(), SharedKeyPool::instance(), client);

    proto::router::RouterToClient message;
    proto::router::ConnectionOffer* offer = message.mutable_connection_offer();
    offer->Swap(&built.offer);
    offer->set_request_id(request_id);

    if (offer->error_code() != proto::router::kErrorOk)
    {
        sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
        return;
    }

    // The relay must learn that one of its keys is gone, or it would keep announcing it.
    RelayWorker* relay_worker = CoreApplication::findWorker<RelayWorker>();
    CHECK(relay_worker);
    relay_worker->notifyKeyUsed(built.relay_session_id, built.relay_key_id);

    // The host could disconnect before the offer reaches its worker; the offer is then dropped
    // there and the consumed relay key is lost, which is acceptable - relays replenish the
    // pool continuously.
    HostWorker* host_worker = CoreApplication::findWorker<HostWorker>();
    CHECK(host_worker);

    CLOG(INFO) << "Sending connection offer to host";
    host_worker->sendConnectionOffer(host_id, *offer);

    CLOG(INFO) << "Sending connection offer to client";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status)
{
    proto::router::RouterToClient message;
    proto::router::HostStatus* host_status = message.mutable_host_status();
    host_status->set_request_id(check_host_status.request_id());

    std::optional<SharedHosts::Host> host_info = SharedHosts::instance().find(check_host_status.host_id());
    if (host_info.has_value())
    {
        host_status->set_error_code(proto::router::kErrorOk);
        host_status->mutable_version()->CopyFrom(serialize(host_info->version));
    }
    else
    {
        const std::string_view error_code = database_.checkHostEntry(check_host_status.host_id());
        host_status->set_error_code(error_code == proto::router::kErrorOk ?
                                    proto::router::kErrorHostOffline : error_code);
    }

    CLOG(INFO) << "Sending host status:" << *host_status;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readHostListRequest(const proto::router::HostListRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::HostList* result = message.mutable_host_list();
    result->set_request_id(request.request_id());

    handleHostList(database_, requestCaller(), request, result);

    // Mark currently connected hosts as online.
    for (proto::router::Host& host : *result->mutable_host())
        host.set_online(SharedHosts::instance().contains(host.host_id()));

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readHostSearchRequest(const proto::router::HostSearchRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::HostSearchResult* result = message.mutable_host_search_result();
    result->set_request_id(request.request_id());

    handleHostSearch(database_, requestCaller(), request, result);

    // Mark currently connected hosts as online.
    for (proto::router::Host& host : *result->mutable_host())
        host.set_online(SharedHosts::instance().contains(host.host_id()));

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readTempHostListRequest(const proto::router::TempHostListRequest& request)
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
void ClientOperator::readWorkspaceListRequest(const proto::router::WorkspaceListRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::WorkspaceList* list = message.mutable_workspace_list();
    list->set_request_id(request.request_id());

    handleWorkspaceList(database_, requestCaller(), request, list);

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readGroupListRequest(const proto::router::GroupListRequest& request)
{
    proto::router::RouterToClient message;
    proto::router::GroupList* result = message.mutable_group_list();
    result->set_request_id(request.request_id());

    handleGroupList(database_, requestCaller(), request, result);

    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ClientOperator::readChangePasswordRequest(const proto::router::ChangePasswordRequest& request)
{
    const RequestResult handled = handleChangePassword(database_, requestCaller(), request);

    proto::router::RouterToClient message;
    proto::router::ChangePasswordResult* result = message.mutable_change_password_result();
    result->set_request_id(request.request_id());
    result->set_error_code(handled.error_code);
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));

    applyRequestResult(handled);

    if (handled.error_code != proto::router::kErrorOk)
        return;

    CLOG(INFO) << "User" << userName() << "rotated own credentials";

    // This request always rotates the password (tokens are revoked in the transaction). Drop the
    // user's other live sessions but keep this one.
    emit sig_stopClients(userId(), {}, sessionId());

    // The rotation revoked every device token, including this session's. Re-run the 2FA stage
    // exactly as on a fresh connection: clear the completion flag and re-challenge. The client
    // must pass 2FA again before it gets the new LoginResult (sent by completeTwoFactor); a failed
    // attempt tears the session down inside readTwoFactorResponse.
    two_factor_completed_ = false;
    token_id_ = 0;
    doTwoFactorChallenge();
}
