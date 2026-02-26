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
#include "base/net/tcp_channel.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

class QWinEventNotifier;

namespace base {
class Location;
} // namespace base

namespace host {

class DesktopManager final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopManager(QObject* parent = nullptr);
    ~DesktopManager() final;

    enum class ProcessState
    {
        STOPPED,
        STARTING,
        ERROR_OCURRED,
        STARTED
    };
    Q_ENUM(ProcessState)

    static DesktopManager* instance();
    static QString filePath();

    base::SessionId sessionId() const;
    const QString& ipcChannelName() const;

public slots:
    void start();
    void onClientStarted();
    void onClientFinished();

signals:
    void sig_attached(const QString& ipc_channel_name);
    void sig_dettached();

private slots:
    void onUserSessionEvent(quint32 event_type, quint32 session_id);
    void onProcessStateChanged(host::DesktopManager::ProcessState state);
    void onRestartTimeout();
    void onAttachTimeout();

private:
    void attach(const base::Location& location, base::SessionId session_id);
    void dettach(const base::Location& location);
    void startProcess(base::SessionId session_id, const QString& ipc_channel_name);
    void stopProcess();

    static thread_local DesktopManager* instance_;

    base::SessionId session_id_ = base::kInvalidSessionId;
    QString ipc_channel_name_;
    bool is_console_ = true;

    QTimer* restart_timer_ = nullptr;
    QTimer* attach_timer_ = nullptr;

    quint32 client_count_ = 0;

#if defined(Q_OS_WINDOWS)
    base::ScopedHandle process_;
    base::ScopedHandle thread_;

    QWinEventNotifier* process_notifier_ = nullptr;
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
    pid_t pid_;
#endif // defined(Q_OS_LINUX)

    ProcessState process_state_ = ProcessState::STOPPED;

    Q_DISABLE_COPY_MOVE(DesktopManager)
};

} // namespace host

#endif // HOST_DESKTOP_MANAGER_H
