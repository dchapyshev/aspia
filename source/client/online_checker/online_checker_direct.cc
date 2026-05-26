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

#include "client/online_checker/online_checker_direct.h"

#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "build/build_config.h"
#include "proto/key_exchange.h"
#include "proto/peer.h"

namespace {

const qsizetype kNumberOfParallelTasks = 30;
const std::chrono::seconds kTimeout { 15 };

} // namespace

class OnlineCheckerDirect::Instance final : public QObject
{
public:
    Instance(const HostConfig& host, QObject* parent);
    ~Instance() final;

    void start();
    qint64 entryId() const { return host_.id(); }

private slots:
    void onTcpAuthenticated();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);

private:
    void onFinished(const Location& location, bool online);

    const HostConfig host_;

    TcpChannel* tcp_channel_ = nullptr;
    QTimer timer_;
    bool finished_ = false;
};

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::Instance::Instance(const HostConfig& host, QObject* parent)
    : QObject(parent),
      host_(host)
{
    timer_.setSingleShot(true);

    connect(&timer_, &QTimer::timeout, this, [this]()
    {
        LOG(TRACE) << "Timeout for host:" << host_.id();
        onFinished(FROM_HERE, false);
    });

    timer_.start(kTimeout);
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::Instance::~Instance()
{
    finished_ = true;
    timer_.stop();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::start()
{
    Address address = Address::fromString(host_.address(), DEFAULT_HOST_TCP_PORT);

    LOG(TRACE) << "Starting connection to" << address.host() << ":" << address.port()
               << "(host:" << host_.id() << ")";

    ClientAuthenticator* authenticator = new ClientAuthenticator();
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setUserName(host_.username());
    authenticator->setPassword(SecureString(host_.password()));
    authenticator->setSessionType(proto::peer::SESSION_TYPE_DESKTOP);
    authenticator->setProbe(true);

    tcp_channel_ = new TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &TcpChannel::sig_authenticated, this, &Instance::onTcpAuthenticated);
    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Instance::onTcpErrorOccurred);

    tcp_channel_->connectTo(address.host(), address.port(), TcpChannel::Seconds(10));
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onTcpAuthenticated()
{
    LOG(TRACE) << "Authentication succeeded (host:" << host_.id() << ")";
    onFinished(FROM_HERE, true);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onTcpErrorOccurred(TcpChannel::ErrorCode /* error_code */)
{
    LOG(TRACE) << "Connection aborted for host:" << host_.id();
    onFinished(FROM_HERE, false);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onFinished(const Location& location, bool online)
{
    LOG(TRACE) << "Finished:" << location;

    if (finished_)
        return;

    finished_ = true;
    timer_.stop();

    OnlineCheckerDirect* checker = static_cast<OnlineCheckerDirect*>(parent());
    checker->onChecked(host_.id(), online);
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::OnlineCheckerDirect(const HostList& hosts, QObject* parent)
    : QObject(parent),
      pending_queue_(hosts)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::~OnlineCheckerDirect()
{
    LOG(TRACE) << "Dtor";

    pending_queue_.clear();
    work_queue_.clear();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::start()
{
    if (pending_queue_.isEmpty())
    {
        LOG(TRACE) << "No hosts in list";
        onFinished(FROM_HERE);
        return;
    }

    qsizetype count = std::min(pending_queue_.size(), kNumberOfParallelTasks);
    while (count != 0)
    {
        const HostConfig& host = pending_queue_.front();
        Instance* instance = new Instance(host, this);

        LOG(TRACE) << "Instance for" << host.id() << "is created (" << host.address() << ")";
        work_queue_.emplace_back(instance);
        pending_queue_.pop_front();

        --count;
    }

    for (const auto& task : std::as_const(work_queue_))
        task->start();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::onChecked(qint64 entry_id, bool online)
{
    emit sig_checkerResult(entry_id, online);

    if (!pending_queue_.isEmpty())
    {
        const HostConfig& host = pending_queue_.front();
        Instance* instance = new Instance(host, this);

        LOG(TRACE) << "Instance for" << host.id() << "is created (" << host.address() << ")";

        work_queue_.emplace_back(instance);
        instance->start();
        pending_queue_.pop_front();
    }
    else
    {
        for (auto it = work_queue_.begin(), it_end = work_queue_.end(); it != it_end; ++it)
        {
            Instance* instance = *it;

            if (instance->entryId() == entry_id)
            {
                instance->deleteLater();
                work_queue_.erase(it);
                break;
            }
        }

        if (work_queue_.isEmpty())
        {
            LOG(TRACE) << "No more items in queue";
            onFinished(FROM_HERE);
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::onFinished(const Location& location)
{
    LOG(TRACE) << "Finished (from" << location << ")";
    emit sig_checkerFinished();
}
