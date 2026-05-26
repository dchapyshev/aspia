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

#ifndef CLIENT_UI_TAB_H
#define CLIENT_UI_TAB_H

#include <QAction>
#include <QList>
#include <QPair>
#include <QWidget>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class QStatusBar;
class HostConfig;

class Tab : public QWidget
{
    Q_OBJECT

public:
    enum class Type { HOSTS, SESSION, SETTINGS };
    enum class ActionRole { FILE, EDIT, VIEW, ACTION, SESSION_TYPE, HELP };

    // Property name. If set on a QAction to true, the action will be added only to the menu and
    // skipped on the toolbar (e.g. checkable items without an icon).
    static inline constexpr const char* kMenuOnlyProperty = "aspiaMenuOnly";

    explicit Tab(Type type, const QString& object_name, QWidget* parent = nullptr);
    ~Tab() override;

    Type tabType() const;
    bool isClosable() const;

    // Container-managed flag remembering whether this tab was visible in the tab bar before
    // entering fullscreen, so the container can decide whether to put it back on exit.
    bool isVisibleBeforeFullscreen() const;
    void setVisibleBeforeFullscreen(bool was_visible);

    virtual bool isDetachable() const;
    virtual bool isDetached() const;
    virtual void detachToWindow();
    virtual void attachToTab();
    virtual QWidget* detachedWindow() const;
    virtual QByteArray saveState() = 0;
    virtual void restoreState(const QByteArray& state) = 0;
    virtual void activate(QStatusBar* statusbar) = 0;
    virtual void deactivate(QStatusBar* statusbar) = 0;
    virtual bool hasSearchField() const;
    virtual bool hasStatusBar() const;
    virtual QString searchText() const;
    virtual void searchTextChanged(const QString& text);

    using ActionGroupEntry = QPair<ActionRole, QList<QAction*>>;
    virtual QList<ActionGroupEntry> actionGroups() const;

signals:
    void sig_titleChanged(const QString& title);
    void sig_closeRequested();

    // Emitted while the user drags a detached tab as a top-level window with the left mouse
    // button held. The owner uses global_pos to update visual hints (e.g. previewing the drop
    // target in the main tab bar). Detachable subclasses are responsible for emitting these.
    void sig_dragMove(const QPoint& global_pos);
    void sig_dragFinished(const QPoint& global_release_pos);

    // Forwarded session-level requests. The container (e.g. MainWindow) decides how to honor
    // them depending on whether the tab is currently attached or detached.
    void sig_fullscreenRequested(bool enabled);
    void sig_minimizeRequested();
    void sig_showRequested();

    // Emitted when the set of actions returned by actionGroups() may have changed (e.g. a
    // session-level toolbar discovered new monitors). The container (MainWindow) uses this to
    // re-install actions on the global toolbar/menus when this tab is currently active.
    void sig_actionsChanged();

    // Emitted when this tab's ClientWindow asks to launch a secondary session (e.g. desktop
    // launching a file transfer or system info session).
    void sig_connectRequested(const HostConfig& host, proto::peer::SessionType session_type);

    // Emitted when the tab wants the container to clear the search field.
    void sig_clearSearch();

protected:
    void addActions(ActionRole group, const QList<QAction*>& actions);

private:
    Type type_;
    QList<ActionGroupEntry> action_groups_;
    bool visible_before_fullscreen_ = false;

    Q_DISABLE_COPY_MOVE(Tab)
};

#endif // CLIENT_UI_TAB_H
