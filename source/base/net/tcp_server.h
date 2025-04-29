//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_TCP_SERVER_H
#define BASE_NET_TCP_SERVER_H

#include "base/macros_magic.h"
#include "base/memory/local_memory.h"

#include <cstdint>
#include <memory>
#include <queue>

#include <QObject>

namespace base {

class TcpChannel;

class TcpServer final : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewConnection() = 0;
    };

    void start(const QString& listen_interface, uint16_t port, Delegate* delegate);
    void stop();
    bool hasPendingConnections();
    TcpChannel* nextPendingConnection();

    QString listenInterface() const;
    uint16_t port() const;

    static bool isValidListenInterface(const QString& interface);

private:
    void onNewConnection(std::unique_ptr<TcpChannel> channel);

    class Impl;
    base::local_shared_ptr<Impl> impl_;

    Delegate* delegate_ = nullptr;
    std::queue<std::unique_ptr<TcpChannel>> pending_;

    DISALLOW_COPY_AND_ASSIGN(TcpServer);
};

} // namespace base

#endif // BASE_NET_TCP_SERVER_H
