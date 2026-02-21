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

#include <QMap>
#include <QObject>
#include <QPointer>

#include "base/location.h"
#include "base/ipc/ipc_channel.h"
#include "base/net/tcp_channel.h"
#include "host/desktop_process.h"

namespace host {

class DesktopClient;

class DesktopManager final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopManager(QObject* parent = nullptr);
    ~DesktopManager() final;

    static DesktopManager* instance();

    void onNewChannel(base::TcpChannel* tcp_channel);

public slots:
    void start();

signals:
    void sig_ipcChannelChanged(const QString& name);

private slots:
    void onUserSessionEvent(quint32 event_type, quint32 session_id);
    void onProcessStateChanged(host::DesktopProcess::State state);
    void onRestartTimeout();
    void onAttachTimeout();
    void onClientFinished();

private:
    void attach(const base::Location& location, base::SessionId session_id);
    void dettach(const base::Location& location);

    static thread_local DesktopManager* instance_;

    base::SessionId session_id_;
    QString ipc_channel_name_;
    bool is_console_ = true;

    QPointer<DesktopProcess> process_;

    QTimer* restart_timer_ = nullptr;
    QTimer* attach_timer_ = nullptr;

    QList<DesktopClient*> clients_;

    Q_DISABLE_COPY_MOVE(DesktopManager)
};

} // namespace host

#endif // HOST_DESKTOP_MANAGER_H
