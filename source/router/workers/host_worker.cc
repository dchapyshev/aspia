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

#include "router/workers/host_worker.h"

#include "base/logging.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/net/tcp_server_legacy.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "proto/router_legacy_host.h"
#include "router/database.h"
#include "router/host_ng.h"
#include "router/host_legacy.h"
#include "router/settings.h"
#include "router/shared_hosts.h"
#include "router/workers/client_worker.h"

//--------------------------------------------------------------------------------------------------
HostWorker::HostWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
HostWorker::~HostWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void HostWorker::requestTempHostList(bool with_address, QObject* context, TempHostListCallback callback)
{
    request(context, [this, with_address]() { return doTempHostList(with_address); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void HostWorker::disconnectHost(HostId host_id, QObject* context, ResultCallback callback)
{
    request(context, [this, host_id]() { return doDisconnectHost(host_id); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void HostWorker::removeHost(HostId host_id, QObject* context, RemoveHostCallback callback)
{
    request(context, [this, host_id]() { return doRemoveHost(host_id); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void HostWorker::updateHost(HostId host_id, QObject* context, ResultCallback callback)
{
    request(context, [this, host_id]() { return doUpdateHost(host_id); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void HostWorker::approveHost(HostId host_id, QObject* context, ErrorCodeCallback callback)
{
    request(context, [this, host_id]() { return doApproveHost(host_id); }, std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void HostWorker::sendConnectionOffer(HostId host_id, const proto::router::ConnectionOffer& offer)
{
    post([this, host_id, offer]()
    {
        Host* host = hostByHostId(host_id);
        if (!host)
        {
            LOG(ERROR) << "Host with id" << host_id << "is no longer connected. Offer dropped";
            return;
        }

        if (HostNG* host_ng = dynamic_cast<HostNG*>(host))
        {
            host_ng->sendConnectionOffer(offer);
            return;
        }

        HostLegacy* host_legacy = static_cast<HostLegacy*>(host);

        proto::router::legacy::ConnectionOffer legacy_offer;
        legacy_offer.set_peer_role(proto::router::legacy::ConnectionOffer::HOST);
        legacy_offer.set_error_code(proto::router::legacy::ConnectionOffer::SUCCESS);
        legacy_offer.mutable_relay()->CopyFrom(offer.relay());
        legacy_offer.mutable_host_data()->set_host_id(host_id);

        host_legacy->sendConnectionOffer(legacy_offer);
    });
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onStart()
{
    Settings settings;

    SecureByteArray private_key(settings.hostPrivateKey());
    if (private_key.isEmpty())
    {
        LOG(ERROR) << "The host private key is not specified in the configuration file";
        return;
    }

    QString listen_interface = settings.listenInterface();
    if (!TcpServer::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return;
    }

    quint16 port = settings.port();
    if (!port)
    {
        LOG(ERROR) << "Invalid port specified in configuration file";
        return;
    }

    quint16 legacy_port = settings.legacyPort();
    if (!legacy_port)
    {
        LOG(ERROR) << "Invalid legacy port specified in configuration file";
        return;
    }

    QStringList host_white_list = settings.hostWhiteList();
    if (host_white_list.isEmpty())
        LOG(INFO) << "Connections from all hosts will be allowed";
    else
        LOG(INFO) << "Allowed hosts:" << host_white_list;

    // Hosts are the largest population and reconnect in storms after a network blip (e.g. hundreds
    // of hosts behind one corporate NAT coming back at once), so they get the most generous caps.
    static constexpr int kMaxPendingConnections = 100;
    static constexpr int kMaxConnectionsPerMinute = 300;

    server_ = new TcpServer();
    connect(server_, &TcpServer::sig_newConnection, this, &HostWorker::onNewHostConnection);

    server_->setPrivateKey(private_key);
    server_->setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_HOST);
    server_->setMaxPendingConnections(kMaxPendingConnections);
    server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    server_->setWhiteList(host_white_list);
    if (!server_->start(port, listen_interface))
    {
        LOG(ERROR) << "Unable to start host listener";
        server_.reset();
        return;
    }

    legacy_server_ = new TcpServerLegacy();
    connect(legacy_server_, &TcpServerLegacy::sig_newConnection,
            this, &HostWorker::onNewLegacyHostConnection);

    legacy_server_->setPrivateKey(private_key);
    legacy_server_->setAnonymousAccess(
        ServerAuthenticatorLegacy::AnonymousAccess::ENABLE, proto::router::SESSION_TYPE_HOST);
    legacy_server_->setMaxPendingConnections(kMaxPendingConnections);
    legacy_server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    legacy_server_->setWhiteList(host_white_list);
    if (!legacy_server_->start(legacy_port, listen_interface))
    {
        LOG(ERROR) << "Unable to start legacy host listener";
        legacy_server_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onStop()
{
    server_.reset();
    legacy_server_.reset();

    for (auto* host : std::as_const(hosts_))
    {
        host->disconnect();
        delete host;
    }

    hosts_.clear();
    SharedHosts::instance().clear();
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onNewHostConnection()
{
    CHECK(server_);
    while (server_->hasReadyConnections())
    {
        TcpChannel* channel = server_->nextReadyConnection();
        LOG(INFO) << "New host:" << channel->peerAddress();

        HostNG* host = new HostNG(channel, this);
        hosts_.emplace_back(host);
        connect(host, &HostNG::sig_hostIdAssigned, this, &HostWorker::onHostIdAssigned);
        connect(host, &Host::sig_finished, this, &HostWorker::onHostFinished);
        connect(host, &Host::sig_notifyChanged, this, &HostWorker::sig_notify);
        host->start();
    }
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onNewLegacyHostConnection()
{
    CHECK(legacy_server_);
    while (legacy_server_->hasReadyConnections())
    {
        TcpChannel* channel = legacy_server_->nextReadyConnection();
        LOG(INFO) << "New legacy host:" << channel->peerAddress();

        HostLegacy* host = new HostLegacy(channel, this);
        hosts_.emplace_back(host);
        connect(host, &HostLegacy::sig_hostIdAssigned, this, &HostWorker::onHostIdAssigned);
        connect(host, &HostLegacy::sig_hostIdRemoved, this, &HostWorker::onHostIdRemoved);
        connect(host, &Host::sig_finished, this, &HostWorker::onHostFinished);
        connect(host, &Host::sig_notifyChanged, this, &HostWorker::sig_notify);
        host->start();
    }
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onHostFinished()
{
    Host* host = dynamic_cast<Host*>(sender());
    CHECK(host);
    removeHostSession(host);
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onHostIdAssigned(HostId host_id)
{
    QList<Host*> matched_hosts;

    for (Host* host : std::as_const(hosts_))
    {
        HostNG* host_ng = dynamic_cast<HostNG*>(host);
        if (host_ng)
        {
            if (host_ng->hostId() == host_id)
                matched_hosts.append(host);
            continue;
        }

        HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host);
        if (host_legacy)
        {
            if (host_legacy->hasHostId(host_id))
                matched_hosts.append(host);
        }
    }

    if (matched_hosts.size() > 1)
    {
        Host* oldest_host = matched_hosts.first();
        for (int i = 1; i < matched_hosts.size(); ++i)
        {
            if (matched_hosts[i]->startTime() < oldest_host->startTime())
                oldest_host = matched_hosts[i];
        }

        LOG(INFO) << "Duplicate host ID" << host_id << "detected. Disconnecting older session (id"
                  << oldest_host->sessionId() << ")";

        removeHostSession(oldest_host);
    }

    publishHostState(host_id);
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onHostIdRemoved(HostId host_id)
{
    publishHostState(host_id);
}

//--------------------------------------------------------------------------------------------------
void HostWorker::removeHostSession(Host* host)
{
    quint32 flags = ClientWorker::NOTIFY_HOSTS;
    QList<HostId> host_ids;

    if (HostNG* host_ng = dynamic_cast<HostNG*>(host))
    {
        if (host_ng->hostId() != kInvalidHostId)
            host_ids.append(host_ng->hostId());

        if (isTempHostId(host_ng->hostId()))
            flags |= ClientWorker::NOTIFY_TEMP_HOSTS;
    }
    else if (HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host))
    {
        host_ids = host_legacy->hostIdList();
    }

    if (host)
    {
        host->disconnect();
        host->deleteLater();
        hosts_.removeOne(host);
    }

    for (HostId host_id : std::as_const(host_ids))
        publishHostState(host_id);

    emit sig_notify(flags);
}

//--------------------------------------------------------------------------------------------------
Host* HostWorker::hostByHostId(HostId host_id)
{
    for (Host* host : std::as_const(hosts_))
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

//--------------------------------------------------------------------------------------------------
void HostWorker::publishHostState(HostId host_id)
{
    if (Host* host = hostByHostId(host_id))
        SharedHosts::instance().add(host_id, host->version(), host->address());
    else
        SharedHosts::instance().remove(host_id);
}

//--------------------------------------------------------------------------------------------------
proto::router::TempHostList HostWorker::doTempHostList(bool with_address) const
{
    proto::router::TempHostList temp_host_list;

    for (Host* host : std::as_const(hosts_))
    {
        HostNG* host_ng = dynamic_cast<HostNG*>(host);
        if (!host_ng || !isTempHostId(host_ng->hostId()))
            continue;

        proto::router::TempHost* temp_host = temp_host_list.add_host();
        temp_host->set_temp_id(host_ng->hostId());
        temp_host->set_computer_name(host_ng->computerName());
        temp_host->set_version(host_ng->version().toString().toStdString());
        temp_host->set_os_name(host_ng->osName());

        if (with_address)
            temp_host->set_address(host_ng->address());
    }

    return temp_host_list;
}

//--------------------------------------------------------------------------------------------------
bool HostWorker::doDisconnectHost(HostId host_id)
{
    if (host_id == kAllHostsId)
    {
        while (!hosts_.isEmpty())
            removeHostSession(hosts_.first());
        return true;
    }

    Host* host = hostByHostId(host_id);
    if (!host)
        return false;

    removeHostSession(host);
    return true;
}

//--------------------------------------------------------------------------------------------------
HostWorker::RemoveHostResult HostWorker::doRemoveHost(HostId host_id)
{
    RemoveHostResult result;
    result.error_code = proto::router::kErrorInternalError;

    Database& database = Database::instance();
    if (!database.isValid())
    {
        LOG(ERROR) << "Failed to connect to database";
        return result;
    }

    if (!database.scheduleHostRemoval(host_id))
    {
        LOG(ERROR) << "Failed to schedule host removal for host_id:" << host_id;
        return result;
    }

    // The hosts row is now in hosts_remove; if the host is online send it the remove command and
    // let HostNG finalize the hosts_remove row on disconnect. Legacy hosts have no router->host
    // remove command, so remove the id from the live legacy session and finalize the pending row
    // immediately. Offline legacy hosts are handled on the next HostIdRequest.
    Host* host = hostByHostId(host_id);
    result.error_code = proto::router::kErrorOk;
    result.scheduled = true;
    result.online = host != nullptr;

    if (HostNG* host_ng = dynamic_cast<HostNG*>(host))
    {
        host_ng->sendRemoveCommand();
    }
    else if (HostLegacy* host_legacy = dynamic_cast<HostLegacy*>(host))
    {
        if (host_legacy->removeHostId(host_id))
        {
            if (!database.finalizeHostRemoval(host_id))
            {
                LOG(ERROR) << "Failed to finalize removal for legacy host_id:" << host_id;
                result.error_code = proto::router::kErrorInternalError;
            }

            if (host_legacy->hostIdList().isEmpty())
                removeHostSession(host_legacy);
        }
    }

    emit sig_notify(ClientWorker::NOTIFY_HOSTS);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool HostWorker::doUpdateHost(HostId host_id)
{
    HostNG* host = dynamic_cast<HostNG*>(hostByHostId(host_id));
    if (!host)
        return false;

    host->sendUpdateCommand();
    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostWorker::doApproveHost(HostId host_id)
{
    HostNG* host = dynamic_cast<HostNG*>(hostByHostId(host_id));
    if (!host)
        return proto::router::kErrorInvalidEntryId;

    Database& database = Database::instance();
    if (!database.isValid())
    {
        LOG(ERROR) << "Failed to connect to database";
        return proto::router::kErrorInternalError;
    }

    if (!database.addHost(host->keyHash(), host->hardwareId()))
    {
        LOG(ERROR) << "Failed to persist approved host_id:" << host_id;
        return proto::router::kErrorInternalError;
    }

    // The key is now persistent. Drop the temporary session so the host reconnects and receives
    // its permanent id via EXISTING_ID. Persist first, then drop: dropping before the row exists
    // would make the reconnect ask for a new temporary id.
    removeHostSession(host);

    emit sig_notify(ClientWorker::NOTIFY_HOSTS | ClientWorker::NOTIFY_TEMP_HOSTS);
    return proto::router::kErrorOk;
}
