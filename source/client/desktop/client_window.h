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

#ifndef CLIENT_DESKTOP_CLIENT_WINDOW_H
#define CLIENT_DESKTOP_CLIENT_WINDOW_H

#include "client/config.h"
#include "client/session_state.h"
#include "client/desktop/tab.h"
#include "client/workers/network_worker.h"

#include <QPointer>
#include <QWidget>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class QAction;
class QTimer;
class SessionKeeper;
class StatusDialog;
class Worker;
class WorkerManager;

class ClientWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ClientWindow(proto::peer::SessionType session_type, QWidget* parent = nullptr);
    virtual ~ClientWindow() override;

    // Connects to a host.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    bool connectToHost(HostConfig host, const QString& display_name);

    std::shared_ptr<SessionState> sessionState() { return session_state_; }
    proto::peer::SessionType sessionType() const { return session_type_; }

    // Notifies the session that it has been embedded in a tab (tabbed=true) or detached into a
    // top-level window (tabbed=false). Subclasses override to hide their own session-level
    // toolbars while in tabbed mode, since their actions are exposed via tabActionGroups().
    virtual void setTabbedMode(bool tabbed);

    // Returns session-specific actions to be hosted by the container's toolbar/menus while the
    // session is embedded in a tab. The default implementation returns an empty list.
    virtual QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const;

    // Re-reads the relevant client settings and applies them to the running session. Called by
    // the container after the user changes settings in the global Settings dialog. The default
    // implementation does nothing; sessions that read from Settings (e.g. DesktopWindow) override.
    virtual void applySettings();

    // Serializes session-window-specific UI state (e.g. desktop toolbar pin/scale/pause flags) to
    // an opaque blob persisted by the container as part of Tab state. The default implementation
    // returns an empty blob; sessions with persistent UI state (e.g. DesktopWindow) override.
    virtual QByteArray saveState() const;
    virtual void restoreState(const QByteArray& state);

signals:
    void sig_stop();
    void sig_startConnection(std::shared_ptr<SessionState> session_state);
    void sig_sessionReady();
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);

    // Emitted when the connection to the host has been successfully established. The container
    // uses this to create the session tab, so that during connection only the status dialog is
    // shown and the tab appears only after a successful connect.
    void sig_connected();

    // Emitted on each drag-poll tick while the user is dragging this widget as a top-level
    // window with the left mouse button held. The owner uses global_pos to update visual hints
    // (e.g. previewing the destination in the main tab bar).
    void sig_dragMove(const QPoint& global_pos);

    // Emitted when the user finishes a native window-drag of this widget while it is top-level
    // (i.e. detached from a tab). The owner inspects global_release_pos to decide whether to
    // re-attach the session into the main tab bar.
    void sig_dragFinished(const QPoint& global_release_pos);

    // Asks the container to toggle fullscreen for this session. The container decides how to do
    // it (e.g. tabbed session detaches into a top-level window first, then goes fullscreen).
    void sig_fullscreenRequested(bool enabled);

    // Asks the container to minimize the session. Tabbed containers ignore this.
    void sig_minimizeRequested();

    // Asks the container to bring the session to the user's attention (raise / activate / make
    // current tab). Emitted when the connection is established.
    void sig_showRequested();

    // Emitted when the set of actions returned by tabActionGroups() may have changed (e.g. the
    // host reported a new monitor configuration). The container re-installs the actions on the
    // global toolbar/menus.
    void sig_actionsChanged();

    // Emitted when this client window asks to launch a secondary session (e.g. desktop launching
    // a file transfer or system info session).
    void sig_connectRequested(const HostConfig& host, proto::peer::SessionType session_type);

protected:
    virtual void onInternalReset() = 0;

    // Called for every connection attempt before the workers are started. The session registers
    // its workers here through addWorker() and connects to the network worker channel signals.
    virtual void onRegisterWorkers() = 0;

    // Connection established, authentication passed. Set up the session and show the window;
    // incoming messages start arriving after this call returns.
    virtual void onSessionStarted() = 0;

    // Registers a worker. May be called only from onRegisterWorkers().
    void addWorker(std::unique_ptr<Worker> worker);

    // Sends outgoing session message.
    void sendMessage(quint8 channel_id, const QByteArray& buffer);

    NetworkWorker* networkWorker() const;
    bool isLegacy() const;

    // Returns actions that launch every session type except this window's own, targeting the
    // current host, in a fixed order. Built once on construction. The default tabActionGroups()
    // exposes them in the ACTION group; subclasses that override tabActionGroups() insert them
    // where appropriate.
    const QList<QAction*>& sessionConnectActions() const { return session_connect_actions_; }

    // QWidget implementation.
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

public slots:
    void onStatusChanged(NetworkWorker::Status status, const QVariant& data);

private slots:
    void onDragPoll();
    void onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onNetworkConnected();

private:
    void setClientTitle(const HostConfig& host, proto::peer::SessionType session_type);
    void onErrorOccurred(const QString& message);
    // For relay-path sessions: fetches a ConnectionOffer from the router (via Router) and
    // handles the response inline. Called both for the initial connect and for every reconnect
    // attempt after host disconnects.
    void fetchConnectionOffer();
    void startNewSession();

    // Builds session_connect_actions_ for every session type except session_type_.
    void createSessionConnectActions();

    const proto::peer::SessionType session_type_;
    std::shared_ptr<SessionState> session_state_;
    StatusDialog* status_dialog_ = nullptr;
    QTimer* drag_poll_timer_ = nullptr;
    QTimer* reconnect_timeout_timer_ = nullptr;
    QList<QAction*> session_connect_actions_;

    std::unique_ptr<WorkerManager> worker_manager_;
    QPointer<NetworkWorker> network_worker_;
    SessionKeeper* session_keeper_ = nullptr;
    bool is_legacy_mode_ = false;
};

#endif // CLIENT_DESKTOP_CLIENT_WINDOW_H
