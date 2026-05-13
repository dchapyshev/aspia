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

#ifndef BASE_IPC_IPC_SERVER_H
#define BASE_IPC_IPC_SERVER_H

#include <QQueue>
#include <QObject>

#include <asio/io_context.hpp>

#include <array>
#include <memory>

class IpcChannel;
class Location;

class IpcServer final : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer();

    // Access mode for the IPC server pipe.
    // Controls which security descriptor is applied on Windows and which file
    // permissions are applied on UNIX.
    enum class AccessMode
    {
        // Any authenticated user can connect. Required when the legitimate client runs under
        // an arbitrary logged-on user's token (e.g. UI agent). On Windows the pipe is also
        // labeled with Medium mandatory IL so Low/Untrusted-IL processes cannot open it.
        INTERACTIVE_USER,

        // Only the specified user SID may connect (in addition to the server itself).
        // Required when the legitimate client runs under a known user's token (e.g. file_agent).
        // On Windows |target_user_sid| must be a valid SID string (S-1-...).
        SPECIFIC_USER,

        // Only SYSTEM may connect. Required when the legitimate client runs as SYSTEM
        // (e.g. desktop_agent). On Windows the pipe is also labeled with System mandatory IL.
        SYSTEM_ONLY
    };
    Q_ENUM(AccessMode)

    static QString createUniqueId();

    bool start(const QString& channel_name,
               AccessMode mode = AccessMode::INTERACTIVE_USER,
               const QString& target_user_sid = QString());
    void stop();
    bool hasPendingConnections();
    IpcChannel* nextPendingConnection();

signals:
    void sig_newConnection();
    void sig_errorOccurred();

private:
    bool runListener(size_t index);
    void onNewConnection(size_t index, IpcChannel* channel);
    void onErrorOccurred(const Location& location);

    asio::io_context& io_context_;
    QString channel_name_;
    AccessMode access_mode_ = AccessMode::INTERACTIVE_USER;
    QString target_user_sid_;

#if defined(Q_OS_WINDOWS)
    static const size_t kListenersCount = 8;
#elif defined(Q_OS_UNIX)
    static const size_t kListenersCount = 1;
#endif

    class Listener;
    std::array<std::shared_ptr<Listener>, kListenersCount> listeners_;
    QQueue<IpcChannel*> pending_;

    Q_DISABLE_COPY_MOVE(IpcServer)
};

#endif // BASE_IPC_IPC_SERVER_H
