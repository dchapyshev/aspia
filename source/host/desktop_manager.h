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

#ifndef HOST_DESKTOP_MANAGER_H
#define HOST_DESKTOP_MANAGER_H

#include <QObject>

#include "base/session_id.h"

class QTimer;

namespace base {
class IpcChannel;
class IpcServer;
class Location;
} // namespace base

namespace host {

class DesktopManager final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopManager(QObject* parent = nullptr);
    ~DesktopManager() final;

    enum class State
    {
        ATTACHED,
        ATTACHING,
        DETTACHED
    };
    Q_ENUM(State)

    static QString filePath();

    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }

    void startAgentClient(const QString& ipc_channel_name);

public slots:
    void onClientStarted();
    void onClientFinished();
    void onClientChannelChanged();
    void onClientSwitchSession(base::SessionId session_id);
    void onUserPause(bool enable);
    void onUserLockMouse(bool enable);
    void onUserLockKeyboard(bool enable);

signals:
    void sig_attached();
    void sig_dettached();

private slots:
    void onUserSessionEvent(quint32 event_type, quint32 session_id);
    void onRestartTimeout();
    void onAttachTimeout();

    // Slots for base::IpcServer.
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    // Slots for base::IpcChannel.
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);

private:
    void attach(const base::Location& location, base::SessionId session_id);
    void dettach(const base::Location& location);
    bool startProcess(const QString& ipc_channel_name);
    void sendMessage(const QByteArray& buffer);

    State state_ = State::DETTACHED;
    base::SessionId session_id_ = base::kInvalidSessionId;
    bool is_console_ = true;

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;

    QTimer* restart_timer_ = nullptr;
    QTimer* attach_timer_ = nullptr;

    int client_count_ = 0;

    Q_DISABLE_COPY_MOVE(DesktopManager)
};

} // namespace host

#endif // HOST_DESKTOP_MANAGER_H
