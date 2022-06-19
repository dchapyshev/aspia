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

class OnlineCheckerDirect::Instance : public base::NetworkChannel::Listener
{
public:
    Instance();
    ~Instance() override;

    using FinishCallback = std::function<void(int computer_id, bool online)>;

    void start(const std::u16string& address, uint16_t port, FinishCallback finish_callback);

protected:
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    int computer_id_;
    std::unique_ptr<base::NetworkChannel> channel_;
};

void OnlineCheckerDirect::Instance::onConnected()
{
    proto::ClientHello message;

    message.set_encryption(proto::ENCRYPTION_CHACHA20_POLY1305);
    message.set_identify(proto::IDENTIFY_SRP);

    channel_->send(base::serialize(message));
}

void OnlineCheckerDirect::Instance::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{

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
            break;

        default:
        {
            LOG(LS_WARNING) << "Invalid encrition method: " << message.encryption();
        }
        return;
    }
}

void OnlineCheckerDirect::Instance::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

OnlineCheckerDirect::OnlineCheckerDirect()
{
    LOG(LS_INFO) << "Ctor";
}

OnlineCheckerDirect::~OnlineCheckerDirect()
{
    LOG(LS_INFO) << "Dtor";
}

void OnlineCheckerDirect::start(const ComputerList& computers, Delegate* delegate)
{
    computers_ = computers;
    delegate_ = delegate;
    DCHECK(delegate_);

    if (computers_.empty())
    {
        LOG(LS_INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }
}

void OnlineCheckerDirect::onFinished(const base::Location& location)
{
    LOG(LS_INFO) << "Finished (from: " << location.toString() << ")";

    for (const auto& computer : computers_)
        delegate_->onDirectCheckerResult(computer.computer_id, false);

    delegate_->onDirectCheckerFinished();
}

} // namespace client
