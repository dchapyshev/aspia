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

#ifndef BASE_NET_ENET_SOCKET_H
#define BASE_NET_ENET_SOCKET_H

#include <QObject>

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace base {

class EnetSocket final : public QObject
{
    Q_OBJECT

public:
    explicit EnetSocket(QObject* parent = nullptr);
    ~EnetSocket() final;

    static EnetSocket* bind(quint16& port);

    void connectTo(const QString& address, quint16 port);
    bool send(const void* data, int size);
    void close();

signals:
    void sig_ready();
    void sig_dataReceived(const char* data, int size); // Only direct connection.
    void sig_errorOccurred();

protected:
    void timerEvent(QTimerEvent* event) final;

private:
    void init(ENetHost* host);
    void processEvents();

    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;
    int update_timer_id_ = 0;
    bool ready_ = false;

    Q_DISABLE_COPY_MOVE(EnetSocket)
};

} // namespace base

#endif // BASE_NET_ENET_SOCKET_H
