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

#ifndef CLIENT__CLIENT_CHANNEL_H
#define CLIENT__CLIENT_CHANNEL_H

#include "base/macros_magic.h"
#include "client/client_authenticator.h"
#include "net/network_listener.h"
#include "proto/common.pb.h"

#include <QObject>

namespace base {
class TaskRunner;
} // namespace base

namespace net {
class Channel;
class ChannelProxy;
} // namespace net

namespace client {

class Channel
    : public QObject,
      public net::Listener
{
    Q_OBJECT

public:
    explicit Channel(QObject* parent = nullptr);
    ~Channel();

    void connectToHost(std::u16string_view address, uint16_t port,
                       std::u16string_view username, std::u16string_view password,
                       proto::SessionType session_type);

    void disconnectFromHost();
    void resume();

    void send(const base::ByteArray& buffer);

    // Returns the version of the connected peer.
    base::Version peerVersion() const;

signals:
    // Emits when a secure connection is established.
    void connected();

    // Emits when the connection is aborted.
    void disconnected();

    // Emitted when an error occurred. Parameter |message| contains a text description of the error.
    void errorOccurred(const QString& message);

    // Emitted when a new message is received.
    void messageReceived(const base::ByteArray& buffer);

protected:
    // net::Listener implementation.
    void onNetworkConnected() override;
    void onNetworkDisconnected() override;
    void onNetworkError(net::ErrorCode error) override;
    void onNetworkMessage(const base::ByteArray& buffer) override;

private:
    void onAuthenticatorFinished(Authenticator::Result result);

    static QString errorToString(net::ErrorCode error);
    static QString errorToString(Authenticator::Result result);

    std::unique_ptr<base::TaskRunner> qt_runner_;
    std::unique_ptr<base::TaskRunner> io_runner_;
    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<net::ChannelProxy> channel_proxy_;
    std::unique_ptr<Authenticator> authenticator_;

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace client

#endif // CLIENT__CLIENT_CHANNEL_H
