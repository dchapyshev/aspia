//
// PROJECT:         Aspia
// FILE:            network/network_channel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel.h"
#include "network/network_channel_proxy.h"

namespace aspia {

NetworkChannel::NetworkChannel()
{
    proxy_.reset(new NetworkChannelProxy(this));
}

NetworkChannel::~NetworkChannel()
{
    proxy_->WillDestroyCurrentChannel();
    proxy_ = nullptr;
}

std::shared_ptr<NetworkChannelProxy>
NetworkChannel::network_channel_proxy() const
{
    return proxy_;
}

} // namespace aspia
