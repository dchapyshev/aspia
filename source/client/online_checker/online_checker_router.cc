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
#include "proto/router_peer.pb.h"

namespace client {

namespace {

const std::chrono::seconds kTimeout { 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::OnlineCheckerRouter(const RouterConfig& router_config, QObject* parent)
    : QObject(parent),
      router_config_(router_config)
{
    LOG(LS_INFO) << "Ctor";

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
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::start(const ComputerList& computers)
{
    computers_ = computers;

    if (computers_.empty())
    {
        LOG(LS_INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    LOG(LS_INFO) << "Connecting to router...";

    channel_ = std::make_unique<base::TcpChannel>();

    connect(channel_.get(), &base::TcpChannel::sig_connected,
            this, &OnlineCheckerRouter::onTcpConnected);

    channel_->connect(router_config_.address, router_config_.port);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    channel_->setKeepAlive(true);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>();

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(router_config_.username);
    authenticator_->setPassword(router_config_.password);
    authenticator_->setSessionType(proto::ROUTER_SESSION_CLIENT);

    connect(authenticator_.get(), &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();

            connect(channel_.get(), &base::TcpChannel::sig_disconnected,
                    this, &OnlineCheckerRouter::onTcpDisconnected);
            connect(channel_.get(), &base::TcpChannel::sig_messageReceived,
                    this, &OnlineCheckerRouter::onTcpMessageReceived);

            const base::Version& router_version = authenticator_->peerVersion();
            if (router_version >= base::Version::kVersion_2_6_0)
            {
                LOG(LS_INFO) << "Using channel id support";
                channel_->setChannelIdSupport(true);
            }

            // Now the session will receive incoming messages.
            channel_->resume();

            checkNextComputer();
        }
        else
        {
            LOG(LS_ERROR) << "Authentication failed: "
                          << base::Authenticator::errorToString(error_code);
            onFinished(FROM_HERE);
        }

        // Authenticator is no longer needed.
        authenticator_.release()->deleteLater();
    });

    authenticator_->start(std::move(channel_));
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::NetworkChannel::errorToString(error_code) << ")";
    onFinished(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onTcpMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
{
    proto::RouterToPeer message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from router";
        onFinished(FROM_HERE);
        return;
    }

    if (!message.has_host_status())
    {
        LOG(LS_ERROR) << "HostStatus not present in message";
        onFinished(FROM_HERE);
        return;
    }

    bool online = message.host_status().status() == proto::HostStatus::STATUS_ONLINE;
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
        LOG(LS_INFO) << "No more computers";
        onFinished(FROM_HERE);
        return;
    }

    const auto& computer = computers_.front();

    LOG(LS_INFO) << "Checking status for host id " << computer.host_id
                 << " (computer id: " << computer.computer_id << ")";

    proto::PeerToRouter message;
    message.mutable_check_host_status()->set_host_id(computer.host_id);
    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onFinished(const base::Location& location)
{
    LOG(LS_INFO) << "Finished (from: " << location.toString() << ")";

    if (channel_)
        channel_.release()->deleteLater();

    if (authenticator_)
        authenticator_.release()->deleteLater();

    for (const auto& computer : computers_)
        emit sig_checkerResult(computer.computer_id, false);

    emit sig_checkerFinished();
}

} // namespace client
