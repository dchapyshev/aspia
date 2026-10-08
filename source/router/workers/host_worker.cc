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

#include <algorithm>

#include "base/logging.h"
#include "base/crypto/secure_byte_array.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "proto/router.h"
#include "proto/router_constants.h"
#include "router/database.h"
#include "router/host.h"
#include "router/settings.h"
#include "router/shared_hosts.h"
#include "router/workers/client_worker.h"

namespace {

// A queued removal expires in months, so sweeping once a day is often enough and keeps the work
// away from the connection storms the worker also serves.
const Hours kRemovalSweepInterval{ 24 };

// A host asks for its id as soon as it is authenticated, so a connection that has not asked
// serves nobody. Anonymous access means anybody can open one and keep it for as long as it likes.
constexpr Seconds kIdentifyTimeout{ 30 };

} // namespace

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
void HostWorker::requestTempHostList(bool with_address, qint64 offset, qint64 count, QObject* context,
                                     TempHostListCallback callback)
{
    request(context, [this, with_address, offset, count]()
    {
        return doTempHostList(with_address, offset, count);
    }, std::move(callback));
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

        host->sendConnectionOffer(offer);
    });
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onPrepare()
{
    Settings settings;

    SecureByteArray private_key(settings.hostPrivateKey());
    if (private_key.isEmpty())
    {
        LOG(ERROR) << "The host private key is not specified in the configuration file";
        return;
    }

    QString listen_interface = settings.hostListenInterface();
    if (!NetUtils::isValidListenInterface(listen_interface))
    {
        LOG(ERROR) << "Invalid listen interface address";
        return;
    }

    quint16 port = settings.hostPort();
    if (!port)
    {
        LOG(ERROR) << "Invalid port specified in configuration file";
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

    listen_interface_ = listen_interface;
    listen_port_ = port;
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onStart()
{
    if (!server_)
        return;

    if (!server_->start(listen_port_, listen_interface_))
    {
        LOG(ERROR) << "Unable to start host listener";
        server_.reset();
    }
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onStop()
{
    server_.reset();

    for (auto* host : std::as_const(hosts_))
    {
        host->disconnect();
        delete host;
    }

    hosts_.clear();
    hosts_by_id_.clear();
    SharedHosts::instance().clear();
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onTimer(TimePoint now)
{
    std::vector<Host*> silent;

    for (Host* host : std::as_const(hosts_))
    {
        if (now - host->startTime() < kIdentifyTimeout)
            continue;

        if (host->hostId() == kInvalidHostId)
            silent.emplace_back(host);
    }

    for (Host* host : std::as_const(silent))
    {
        LOG(WARNING) << "Connection without a host id dropped (id" << host->sessionId() << ")";
        removeHostSession(host);
    }

    if (now < next_removal_sweep_)
        return;
    next_removal_sweep_ = now + kRemovalSweepInterval;

    Database& database = Database::instance();
    if (database.isValid())
        database.pruneExpiredHostRemovals();
}

//--------------------------------------------------------------------------------------------------
void HostWorker::onNewHostConnection()
{
    CHECK(server_);
    while (server_->hasReadyConnections())
    {
        TcpChannel* channel = server_->nextReadyConnection();
        LOG(INFO) << "New host:" << channel->peerAddress();

        Host* host = new Host(Database::instance(), channel, this);
        hosts_.emplace_back(host);
        connect(host, &Host::sig_hostIdAssigned, this, &HostWorker::onHostIdAssigned);
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
    Host* host = dynamic_cast<Host*>(sender());
    CHECK(host);

    // The holder in the index got the id earlier, so it is the stale one. It is replaced first,
    // and its removal below skips the entry of the newcomer and publishes the newcomer.
    Host* stale_host = hostByHostId(host_id);
    hosts_by_id_[host_id] = host;

    if (stale_host && stale_host != host)
    {
        LOG(INFO) << "Duplicate host ID" << host_id << "detected. Disconnecting stale predecessor (id"
                  << stale_host->sessionId() << ")";

        removeHostSession(stale_host);
    }

    publishHostState(host_id);
}

//--------------------------------------------------------------------------------------------------
void HostWorker::removeHostSession(Host* host)
{
    const HostId host_id = host->hostId();

    quint32 flags = ClientWorker::NOTIFY_HOSTS;
    if (isTempHostId(host_id))
        flags |= ClientWorker::NOTIFY_TEMP_HOSTS;

    host->stop();
    host->deleteLater();
    std::erase(hosts_, host);

    if (host_id != kInvalidHostId)
    {
        // The id may already be held by the newcomer that displaced this host. Only the own
        // entries go away, and the publication below then keeps the newcomer announced.
        if (hostByHostId(host_id) == host)
            hosts_by_id_.erase(host_id);

        publishHostState(host_id);
    }

    emit sig_notify(flags);
}

//--------------------------------------------------------------------------------------------------
Host* HostWorker::hostByHostId(HostId host_id)
{
    const auto it = hosts_by_id_.find(host_id);
    return it != hosts_by_id_.end() ? it->second : nullptr;
}

//--------------------------------------------------------------------------------------------------
void HostWorker::publishHostState(HostId host_id)
{
    Host* host = hostByHostId(host_id);

    // A host that was told to remove itself is not announced, whatever it does with the command.
    // An announced host is one a client may be offered a connection to, and its record is already
    // on its way out of the database.
    if (host && host->isRemoveCommandSent())
        host = nullptr;

    if (host)
        SharedHosts::instance().add(host_id, host->version(), host->address());
    else
        SharedHosts::instance().remove(host_id);
}

//--------------------------------------------------------------------------------------------------
proto::router::TempHostList HostWorker::doTempHostList(bool with_address, qint64 offset, qint64 count) const
{
    proto::router::TempHostList temp_host_list;

    if (offset < 0 || count < 1 || count > proto::router::kMaxTempHostPageSize)
    {
        LOG(ERROR) << "Invalid temporary host list page: offset" << offset << "count" << count;
        temp_host_list.set_error_code(proto::router::kErrorInvalidRequest);
        return temp_host_list;
    }

    std::vector<Host*> temp_hosts;
    for (Host* host : std::as_const(hosts_))
    {
        if (isTempHostId(host->hostId()))
            temp_hosts.emplace_back(host);
    }

    // The connections are kept in the order they arrived. The page has to name the same records
    // every time it is asked for, so the order is fixed here.
    std::sort(temp_hosts.begin(), temp_hosts.end(),
              [](const Host* a, const Host* b) { return a->hostId() < b->hostId(); });

    temp_host_list.set_error_code(proto::router::kErrorOk);
    temp_host_list.set_total_count(static_cast<qint64>(temp_hosts.size()));

    const qint64 wall_now = secondsSinceEpoch();
    const TimePoint monotonic_now = Clock::now();

    const size_t begin = std::min(static_cast<size_t>(offset), temp_hosts.size());
    const size_t end = std::min(begin + static_cast<size_t>(count), temp_hosts.size());

    for (size_t i = begin; i < end; ++i)
    {
        Host* host = temp_hosts[i];

        proto::router::TempHost* temp_host = temp_host_list.add_host();
        temp_host->set_temp_id(host->hostId());
        temp_host->set_computer_name(host->computerName());
        temp_host->set_version(host->version().toString().toStdString());
        temp_host->set_os_name(host->osName());
        temp_host->set_portable(host->isPortable());
        temp_host->set_connect_time(
            wall_now - DurationCast<Seconds>(monotonic_now - host->startTime()).count());

        if (with_address)
            temp_host->set_address(host->address());
    }

    return temp_host_list;
}

//--------------------------------------------------------------------------------------------------
bool HostWorker::doDisconnectHost(HostId host_id)
{
    if (host_id == kAllHostsId)
    {
        while (!hosts_.empty())
            removeHostSession(hosts_.front());
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
    // let Host finalize the hosts_remove row on disconnect.
    Host* host = hostByHostId(host_id);
    result.error_code = proto::router::kErrorOk;
    result.scheduled = true;
    result.online = host != nullptr;

    if (host)
    {
        host->sendRemoveCommand();

        // The connection lives on until the host carries the command out, and an announced host
        // is one a client may be offered a connection to.
        publishHostState(host_id);
    }

    emit sig_notify(ClientWorker::NOTIFY_HOSTS);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool HostWorker::doUpdateHost(HostId host_id)
{
    Host* host = hostByHostId(host_id);
    if (!host)
        return false;

    host->sendUpdateCommand();
    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view HostWorker::doApproveHost(HostId host_id)
{
    if (!isTempHostId(host_id))
    {
        LOG(ERROR) << "Attempt to approve a permanent host id:" << host_id;
        return proto::router::kErrorInvalidEntryId;
    }

    Host* host = hostByHostId(host_id);
    if (!host)
        return proto::router::kErrorInvalidEntryId;

    if (host->isPortable())
    {
        LOG(ERROR) << "Attempt to approve a portable host:" << host_id;
        return proto::router::kErrorAccessDenied;
    }

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

    // The key is now persistent. Drop the temporary connection so the host reconnects and receives
    // its permanent id via EXISTING_ID. Persist first, then drop: dropping before the row exists
    // would make the reconnect ask for a new temporary id.
    removeHostSession(host);

    emit sig_notify(ClientWorker::NOTIFY_HOSTS | ClientWorker::NOTIFY_TEMP_HOSTS);
    return proto::router::kErrorOk;
}
