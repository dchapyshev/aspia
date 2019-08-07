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

#ifndef CLIENT__CLIENT_H
#define CLIENT__CLIENT_H

#include "base/macros_magic.h"
#include "base/version.h"
#include "client/client_authenticator.h"
#include "client/connect_data.h"
#include "net/network_error.h"
#include "net/network_listener.h"

#include <QObject>

namespace net {
class Channel;
} // namespace net

namespace client {

class Client : public QObject, public net::Listener
{
    Q_OBJECT

public:
    Client(const ConnectData& connect_data, QObject* parent);
    virtual ~Client();

    // Starts session.
    void start();

    ConnectData& connectData() { return connect_data_; }

    // Returns the version of the connected host.
    base::Version peerVersion() const;

    // Returns the version of the current client.
    base::Version version() const;

signals:
    // Indicates that the session is started.
    void started();

    // Indicates an error in the session.
    void errorOccurred(const QString& message);

protected:
    // Sends outgoing message.
    void sendMessage(const google::protobuf::MessageLite& message);

    // net::Listener implementation.
    void onNetworkConnected() override;
    void onNetworkDisconnected() override;
    void onNetworkError(net::ErrorCode error_code) override;

private:
    static QString errorToString(net::ErrorCode error_code);
    static QString errorToString(Authenticator::Result result);

    ConnectData connect_data_;

    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<net::ChannelProxy> channel_proxy_;
    std::unique_ptr<Authenticator> authenticator_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace client

#endif // CLIENT__CLIENT_H
