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

#ifndef CLIENT_UI_CLIENT_WINDOW_H
#define CLIENT_UI_CLIENT_WINDOW_H

#include "base/scoped_qpointer.h"
#include "client/client.h"
#include "client/config.h"
#include "client/session_state.h"
#include "client/ui/tab.h"

#include <QWidget>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class QTimer;
class StatusDialog;

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
    virtual Client* createClient() = 0;
    virtual void onInternalReset() = 0;

    // QWidget implementation.
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

public slots:
    void onStatusChanged(Client::Status status, const QVariant& data);

private slots:
    void onDragPoll();

private:
    void setClientTitle(const HostConfig& host, proto::peer::SessionType session_type);
    void onErrorOccurred(const QString& message);
    // For relay-path sessions: fetches a ConnectionOffer from the router (via Router) and
    // handles the response inline. Called both for the initial connect and for every reconnect
    // attempt after host disconnects.
    void fetchConnectionOffer();
    // Destroys the current Client (if any) and creates a fresh one bound to session_state_.
    // Called on the initial start and on every reconnect (a Client is single-use - reconnects
    // discard the old instance and build a new one).
    void startNewClient();

    const proto::peer::SessionType session_type_;
    std::shared_ptr<SessionState> session_state_;
    ScopedQPointer<Client> client_;
    StatusDialog* status_dialog_ = nullptr;
    QTimer* drag_poll_timer_ = nullptr;
    QTimer* reconnect_timeout_timer_ = nullptr;
};

#endif // CLIENT_UI_CLIENT_WINDOW_H
