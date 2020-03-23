//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "proxy/controller.h"

#include "base/logging.h"
#include "net/network_channel.h"

namespace proxy {

Controller::Controller(uint32_t controller_id,
                       std::unique_ptr<SharedPool> shared_pool,
                       std::unique_ptr<net::Channel> channel,
                       Delegate* delegate)
    : controller_id_(controller_id),
      shared_pool_(std::move(shared_pool)),
      channel_(std::move(channel)),
      delegate_(delegate)
{
    DCHECK(channel_ && delegate_);
}

Controller::~Controller() = default;

void Controller::start()
{
    channel_->setListener(this);
    channel_->resume();
}

void Controller::stop()
{
    delegate_ = nullptr;
}

void Controller::onConnected()
{
    NOTREACHED();
}

void Controller::onDisconnected(net::ErrorCode /* error_code */)
{
    if (delegate_)
        delegate_->onControllerFinished(this);
}

void Controller::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
        return;

    // Unsupported messages are ignored.
    if (!incoming_message_.has_key_pool_request())
        return;

    outgoing_message_.Clear();

    // Add the requested number of keys to the pool.
    for (int i = 0; i < incoming_message_.key_pool_request().pool_size(); ++i)
    {
        SessionKey session_key = SessionKey::create();
        if (!session_key.isValid())
            return;

        // Add the key to the outgoing message.
        proto::ProxyKey* key = outgoing_message_.mutable_key_pool()->add_key();

        key->set_type(proto::ProxyKey::TYPE_X25519);
        key->set_encryption(proto::ProxyKey::ENCRYPTION_CHACHA20_POLY1305);
        key->set_public_key(base::toStdString(session_key.publicKey()));
        key->set_iv(base::toStdString(session_key.iv()));

        // Add the key to the pool.
        key->set_key_id(shared_pool_->addKey(controller_id_, std::move(session_key)));
    }

    // Send a message to the router.
    channel_->send(base::serialize(outgoing_message_));
}

void Controller::onMessageWritten()
{
    // Nothing
}

} // namespace proxy
