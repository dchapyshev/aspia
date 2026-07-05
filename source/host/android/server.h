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

#ifndef HOST_ANDROID_SERVER_H
#define HOST_ANDROID_SERVER_H

#include <QList>
#include <QObject>

#include "base/scoped_qpointer.h"

class Client;
class TcpChannel;
class TcpServer;

// Host server for Android, the in-process analog of the desktop Service. It is moved to the
// application I/O thread and driven from there: start()/stop() and the network objects it owns run on
// that thread. For now it only starts the TCP server that accepts incoming direct connections.
class Server final : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);
    ~Server() final;

public slots:
    // Both run on the I/O thread (invoke queued after moveToThread).
    void start();
    void stop();

private slots:
    void onNewConnection();
    void onClientFinished();

private:
    void startClient(TcpChannel* tcp_channel);

    ScopedQPointer<TcpServer> tcp_server_;
    QList<Client*> clients_;

    Q_DISABLE_COPY_MOVE(Server)
};

#endif // HOST_ANDROID_SERVER_H
