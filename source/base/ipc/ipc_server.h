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

#ifndef BASE_IPC_IPC_SERVER_H
#define BASE_IPC_IPC_SERVER_H

#include <QObject>

#include <asio/io_context.hpp>

#include <array>
#include <memory>
#include <queue>

#include "base/macros_magic.h"

namespace base {

class IpcChannel;
class Location;

class IpcServer final : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer();

    static QString createUniqueId();

    bool start(const QString& channel_id);
    void stop();
    bool hasPendingConnections();
    IpcChannel* nextPendingConnection();

signals:
    void sig_newConnection();
    void sig_errorOccurred();

private:
    bool runListener(size_t index);
    void onNewConnection(size_t index, std::unique_ptr<IpcChannel> channel);
    void onErrorOccurred(const Location& location);

    asio::io_context& io_context_;
    QString channel_name_;

#if defined(Q_OS_WINDOWS)
    static const size_t kListenersCount = 8;
#elif defined(Q_OS_UNIX)
    static const size_t kListenersCount = 1;
#endif

    class Listener;
    std::array<std::shared_ptr<Listener>, kListenersCount> listeners_;
    std::queue<std::unique_ptr<IpcChannel>> pending_;

    DISALLOW_COPY_AND_ASSIGN(IpcServer);
};

} // namespace base

#endif // BASE_IPC_IPC_SERVER_H
