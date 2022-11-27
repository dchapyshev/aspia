//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/online_checker_direct.h"

#include "base/location.h"
#include "base/logging.h"
#include "proto/key_exchange.pb.h"

namespace client {

namespace {

const size_t kNumberOfParallelTasks = 30;

} // namespace

class OnlineCheckerDirect::Instance : public base::NetworkChannel::Listener
{
public:
    Instance(int computer_id, const std::u16string& address, uint16_t port);
    ~Instance() override;

    using FinishCallback = std::function<void(int computer_id, bool online)>;

    void start(FinishCallback finish_callback);

protected:
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void onFinished(bool online);

    const int computer_id_;
    const std::u16string address_;
    const uint16_t port_;

    FinishCallback finish_callback_;
    std::unique_ptr<base::NetworkChannel> channel_;
};

OnlineCheckerDirect::Instance::Instance(
    int computer_id, const std::u16string& address, uint16_t port)
    : computer_id_(computer_id),
      address_(address),
      port_(port)
{
    // Nothing
}

OnlineCheckerDirect::Instance::~Instance()
{
    finish_callback_ = nullptr;
    channel_->setListener(nullptr);
    channel_.reset();
}

void OnlineCheckerDirect::Instance::start(FinishCallback finish_callback)
{
    finish_callback_ = std::move(finish_callback);
    DCHECK(finish_callback_);

    channel_ = std::make_unique<base::NetworkChannel>();
    channel_->setListener(this);
    channel_->connect(address_, port_);
}

void OnlineCheckerDirect::Instance::onConnected()
{
    proto::ClientHello message;

    message.set_encryption(proto::ENCRYPTION_CHACHA20_POLY1305);
    message.set_identify(proto::IDENTIFY_SRP);

    channel_->setNoDelay(true);
    channel_->send(base::serialize(message));
}

void OnlineCheckerDirect::Instance::onDisconnected(base::NetworkChannel::ErrorCode /* error_code */)
{
    onFinished(false);
}

void OnlineCheckerDirect::Instance::onMessageReceived(const base::ByteArray& buffer)
{
    proto::ServerHello message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_WARNING) << "Invalid message received";
        return;
    }

    switch (message.encryption())
    {
        case proto::ENCRYPTION_CHACHA20_POLY1305:
        {
            onFinished(true);
        }
        break;

        default:
        {
            LOG(LS_WARNING) << "Invalid encryption method: " << message.encryption();
        }
        return;
    }
}

void OnlineCheckerDirect::Instance::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void OnlineCheckerDirect::Instance::onFinished(bool online)
{
    if (finish_callback_)
    {
        finish_callback_(computer_id_, online);
        finish_callback_ = nullptr;
    }
}

OnlineCheckerDirect::OnlineCheckerDirect()
{
    LOG(LS_INFO) << "Ctor";
}

OnlineCheckerDirect::~OnlineCheckerDirect()
{
    LOG(LS_INFO) << "Dtor";

    delegate_ = nullptr;
    pending_queue_.clear();
    work_queue_.clear();
}

void OnlineCheckerDirect::start(const ComputerList& computers, Delegate* delegate)
{
    pending_queue_ = computers;
    delegate_ = delegate;
    DCHECK(delegate_);

    if (pending_queue_.empty())
    {
        LOG(LS_INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    size_t count = std::min(pending_queue_.size(), kNumberOfParallelTasks);
    while (count != 0)
    {
        const Computer& computer = pending_queue_.front();
        std::unique_ptr<Instance> instance =
            std::make_unique<Instance>(computer.computer_id, computer.address, computer.port);

        work_queue_.emplace_back(std::move(instance));
        pending_queue_.pop_front();
    }

    for (const auto& task : work_queue_)
    {
        task->start(std::bind(&OnlineCheckerDirect::onChecked, this,
                              std::placeholders::_1, std::placeholders::_2));
    }
}

void OnlineCheckerDirect::onChecked(int computer_id, bool online)
{
    if (delegate_)
        delegate_->onDirectCheckerResult(computer_id, online);

    if (pending_queue_.empty())
    {
        onFinished(FROM_HERE);
        return;
    }

    const Computer& computer = pending_queue_.front();
    std::unique_ptr<Instance> instance =
        std::make_unique<Instance>(computer.computer_id, computer.address, computer.port);

    work_queue_.emplace_back(std::move(instance));
    work_queue_.back()->start(std::bind(&OnlineCheckerDirect::onChecked, this,
                                        std::placeholders::_1, std::placeholders::_2));
    pending_queue_.pop_front();
}

void OnlineCheckerDirect::onFinished(const base::Location& location)
{
    LOG(LS_INFO) << "Finished (from: " << location.toString() << ")";
    if (delegate_)
        delegate_->onDirectCheckerFinished();
}

} // namespace client
