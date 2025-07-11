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

#include "client/online_checker/online_checker_direct.h"

#include <QPointer>
#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/net/tcp_channel.h"
#include "build/build_config.h"
#include "proto/key_exchange.h"

namespace client {

namespace {

const qint64 kNumberOfParallelTasks = 30;
const std::chrono::seconds kTimeout { 15 };

} // namespace

class OnlineCheckerDirect::Instance final : public QObject
{
public:
    Instance(int computer_id, const QString& address, quint16 port, QObject* parent);
    ~Instance() final;

    using FinishCallback = std::function<void(int computer_id, bool online)>;

    void start(FinishCallback finish_callback);
    int computerId() const { return computer_id_; }

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);

private:
    void onFinished(const base::Location& location, bool online);

    const int computer_id_;
    const QString address_;
    const quint16 port_;

    FinishCallback finish_callback_;
    QPointer<base::TcpChannel> tcp_channel_;
    QTimer timer_;
    bool finished_ = false;
};

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::Instance::Instance(
    int computer_id, const QString& address, quint16 port, QObject* parent)
    : QObject(parent),
      computer_id_(computer_id),
      address_(address),
      port_(port)
{
    timer_.setSingleShot(true);

    connect(&timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Timeout for computer:" << computer_id_;
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
void OnlineCheckerDirect::Instance::start(FinishCallback finish_callback)
{
    finish_callback_ = std::move(finish_callback);
    DCHECK(finish_callback_);

    LOG(INFO) << "Starting connection to" << address_ << ":" << port_
              << "(computer:" << computer_id_ << ")";

    tcp_channel_ = new base::TcpChannel(this);

    connect(tcp_channel_, &base::TcpChannel::sig_connected, this, &Instance::onTcpConnected);
    connect(tcp_channel_, &base::TcpChannel::sig_disconnected, this, &Instance::onTcpDisconnected);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Instance::onTcpMessageReceived);

    tcp_channel_->connectTo(address_, port_);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onTcpConnected()
{
    LOG(INFO) << "Connection to" << address_ << ":" << port_
              << "established (computer:" << computer_id_ << ")";

    proto::key_exchange::ClientHello message;

    message.set_encryption(proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305);
    message.set_identify(proto::key_exchange::IDENTIFY_SRP);

    proto::peer::Version* version = message.mutable_version();
    version->set_major(ASPIA_VERSION_MAJOR);
    version->set_minor(ASPIA_VERSION_MINOR);
    version->set_patch(ASPIA_VERSION_PATCH);
    version->set_revision(GIT_COMMIT_COUNT);

    tcp_channel_->resume();
    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onTcpDisconnected(base::TcpChannel::ErrorCode /* error_code */)
{
    LOG(INFO) << "Connection aborted for computer:" << computer_id_;
    onFinished(FROM_HERE, false);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::key_exchange::ServerHello message;

    if (!base::parse(buffer, &message))
    {
        LOG(ERROR) << "Invalid message received";
        return;
    }

    switch (message.encryption())
    {
        case proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305:
        {
            LOG(INFO) << "Message received for computer:" << computer_id_;
            onFinished(FROM_HERE, true);
        }
        break;

        default:
        {
            LOG(ERROR) << "Invalid encryption method:" << message.encryption();
        }
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::Instance::onFinished(const base::Location& location, bool online)
{
    LOG(INFO) << "Finished from:" << location;

    if (finished_)
        return;

    finished_ = true;
    timer_.stop();

    finish_callback_(computer_id_, online);
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::OnlineCheckerDirect(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerDirect::~OnlineCheckerDirect()
{
    LOG(INFO) << "Dtor";

    pending_queue_.clear();
    work_queue_.clear();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::start(const ComputerList& computers)
{
    pending_queue_ = computers;

    if (pending_queue_.isEmpty())
    {
        LOG(INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    int count = std::min(pending_queue_.size(), kNumberOfParallelTasks);
    while (count != 0)
    {
        const Computer& computer = pending_queue_.front();

        quint16 port = computer.port;
        if (port == 0)
            port = DEFAULT_HOST_TCP_PORT;

        Instance* instance = new Instance(computer.computer_id, computer.address, port, this);

        LOG(INFO) << "Instance for" << computer.computer_id << "is created (address:"
                  << computer.address << "port:" << port << ")";
        work_queue_.push_back(instance);
        pending_queue_.pop_front();

        --count;
    }

    for (const auto& task : std::as_const(work_queue_))
    {
        task->start(std::bind(&OnlineCheckerDirect::onChecked, this,
                              std::placeholders::_1, std::placeholders::_2));
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::onChecked(int computer_id, bool online)
{
    emit sig_checkerResult(computer_id, online);

    if (!pending_queue_.isEmpty())
    {
        const Computer& computer = pending_queue_.front();
        Instance* instance = new Instance(
            computer.computer_id, computer.address, computer.port, this);

        LOG(INFO) << "Instance for" << computer.computer_id << "is created (address:"
                  << computer.address << "port:" << computer.port << ")";

        work_queue_.push_back(instance);
        work_queue_.back()->start(std::bind(&OnlineCheckerDirect::onChecked, this,
                                            std::placeholders::_1, std::placeholders::_2));
        pending_queue_.pop_front();
    }
    else
    {
        for (auto it = work_queue_.begin(), it_end = work_queue_.end(); it != it_end; ++it)
        {
            Instance* instance = *it;

            if (instance->computerId() == computer_id)
            {
                instance->deleteLater();
                work_queue_.erase(it);
                break;
            }
        }

        if (work_queue_.isEmpty())
        {
            LOG(INFO) << "No more items in queue";
            onFinished(FROM_HERE);
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerDirect::onFinished(const base::Location& location)
{
    LOG(INFO) << "Finished (from" << location << ")";
    emit sig_checkerFinished();
}

} // namespace client
