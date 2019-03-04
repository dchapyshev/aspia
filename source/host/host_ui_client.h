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

#ifndef HOST__HOST_UI_CLIENT_H
#define HOST__HOST_UI_CLIENT_H

#include "base/macros_magic.h"
#include "proto/host.pb.h"

#include <QObject>
#include <QPointer>

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class UiClient : public QObject
{
    Q_OBJECT

public:
    explicit UiClient(QObject* parent = nullptr);
    ~UiClient();

    enum class State { NOT_CONNECTED, CONNECTING, CONNECTED };

    State state() const { return state_; }

    void start();
    void stop();

    void killSession(const std::string& uuid);

signals:
    void disconnected();
    void errorOccurred();
    void connectEvent(const proto::host::ConnectEvent& event);
    void disconnectEvent(const proto::host::DisconnectEvent& event);

private slots:
    void onChannelMessage(const QByteArray& buffer);

private:
    State state_ = State::NOT_CONNECTED;
    ipc::Channel* channel_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UiClient);
};

} // namespace host

#endif // HOST__HOST_UI_CLIENT_H
