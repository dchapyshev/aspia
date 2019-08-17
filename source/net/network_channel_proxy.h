//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef NET__NETWORK_CHANNEL_PROXY_H
#define NET__NETWORK_CHANNEL_PROXY_H

#include "net/network_channel.h"

namespace net {

class ChannelProxy
{
public:
    bool connectToHost(const QString& address, uint16_t port);
    bool disconnectFromHost();

    bool setListener(Listener* listener);
    bool setCryptor(std::unique_ptr<crypto::Cryptor> cryptor);

    bool isConnected() const;
    bool isPaused() const;

    bool pause();
    bool resume();

    QString peerAddress() const;

    bool send(base::ByteArray&& buffer);

private:
    friend class Channel;

    explicit ChannelProxy(Channel* channel);

    // Called directly by Channel::~Channel.
    void willDestroyCurrentChannel();

    Channel* channel_;
    mutable std::mutex channel_lock_;

    DISALLOW_COPY_AND_ASSIGN(ChannelProxy);
};

} // namespace net

#endif // NET__NETWORK_CHANNEL_PROXY_H
