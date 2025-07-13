//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/online_checker/online_checker_router.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_peer.h"

namespace client {

namespace {

const std::chrono::seconds kTimeout { 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::OnlineCheckerRouter(const RouterConfig& router_config, QObject* parent)
    : QObject(parent),
      router_config_(router_config)
{
    LOG(INFO) << "Ctor";

    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]()
    {
        onFinished(FROM_HERE);
    });

    timer_.start(kTimeout);
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::~OnlineCheckerRouter()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::start(const ComputerList& computers)
{
    computers_ = computers;

    if (computers_.empty())
    {
        LOG(INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    LOG(INFO) << "Connecting to router...";

    base::ClientAuthenticator* authenticator = new base::ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setUserName(router_config_.username);
    authenticator->setPassword(router_config_.password);
    authenticator->setSessionType(proto::router::SESSION_TYPE_CLIENT);

    tcp_channel_ = new base::TcpChannel(authenticator, this);

    connect(tcp_channel_, &base::TcpChannel::sig_authenticated,
            this, &OnlineCheckerRouter::onTcpReady);
    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred,
            this, &OnlineCheckerRouter::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
            this, &OnlineCheckerRouter::onTcpMessageReceived);

    tcp_channel_->connectTo(router_config_.address, router_config_.port);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpReady()
{
    LOG(INFO) << "Connection to the router is established";

    // Now the session will receive incoming messages.
    tcp_channel_->resume();
    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Connection to the router is lost (" << error_code << ")";
    onFinished(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::RouterToPeer message;
    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Invalid message from router";
        onFinished(FROM_HERE);
        return;
    }

    if (!message.has_host_status())
    {
        LOG(ERROR) << "HostStatus not present in message";
        onFinished(FROM_HERE);
        return;
    }

    bool online = message.host_status().status() == proto::router::HostStatus::STATUS_ONLINE;
    const Computer& computer = computers_.front();

    emit sig_checkerResult(computer.computer_id, online);

    computers_.pop_front();
    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::checkNextComputer()
{
    if (computers_.empty())
    {
        LOG(INFO) << "No more computers";
        onFinished(FROM_HERE);
        return;
    }

    const auto& computer = computers_.front();

    LOG(INFO) << "Checking status for host id" << computer.host_id
              << "(computer id:" << computer.computer_id << ")";

    proto::router::PeerToRouter message;
    message.mutable_check_host_status()->set_host_id(computer.host_id);
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onFinished(const base::Location& location)
{
    LOG(INFO) << "Finished (from:" << location << ")";

    if (tcp_channel_)
    {
        tcp_channel_->deleteLater();
        tcp_channel_ = nullptr;
    }

    for (const auto& computer : std::as_const(computers_))
        emit sig_checkerResult(computer.computer_id, false);

    emit sig_checkerFinished();
}

} // namespace client
