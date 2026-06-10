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

#include "client/desktop/client_tab.h"

#include <QEvent>
#include <QPointer>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "client/settings.h"
#include "client/desktop/client_window.h"

namespace {

QString tabObjectName(ClientWindow* client_window)
{
    CHECK(client_window);

    switch (client_window->sessionType())
    {
        case proto::peer::SESSION_TYPE_DESKTOP:       return "session_desktop";
        case proto::peer::SESSION_TYPE_FILE_TRANSFER: return "session_file_transfer";
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:   return "session_system_info";
        case proto::peer::SESSION_TYPE_CHAT:          return "session_chat";
        default:                                      return "session";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientTab::ClientTab(ClientWindow* client_window, QWidget* parent)
    : Tab(Type::SESSION, tabObjectName(client_window), parent),
      client_window_(client_window)
{
    setAutoFillBackground(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    client_window_->setParent(this);
    client_window_->installEventFilter(this);
    layout->addWidget(client_window_);
    client_window_->show();
    client_window_->setTabbedMode(true);

    connect(client_window_, &ClientWindow::sig_dragMove, this, &Tab::sig_dragMove);
    connect(client_window_, &ClientWindow::sig_dragFinished, this, &Tab::sig_dragFinished);
    connect(client_window_, &ClientWindow::sig_fullscreenRequested, this, &Tab::sig_fullscreenRequested);
    connect(client_window_, &ClientWindow::sig_minimizeRequested, this, &Tab::sig_minimizeRequested);
    connect(client_window_, &ClientWindow::sig_showRequested, this, &Tab::sig_showRequested);
    connect(client_window_, &ClientWindow::sig_actionsChanged, this, &Tab::sig_actionsChanged);
    connect(client_window_, &ClientWindow::sig_connectRequested, this, &Tab::sig_connectRequested);
}

//--------------------------------------------------------------------------------------------------
ClientTab::~ClientTab()
{
    closing_ = true;

    if (client_window_)
        client_window_->close();
}

//--------------------------------------------------------------------------------------------------
ClientWindow* ClientTab::clientWindow() const
{
    return client_window_;
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::isDetachable() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::isDetached() const
{
    return client_window_ && client_window_->isWindow();
}

//--------------------------------------------------------------------------------------------------
void ClientTab::detachToWindow()
{
    if (!client_window_ || isDetached())
        return;

    layout()->removeWidget(client_window_);
    client_window_->setParent(nullptr, Qt::Window);
    client_window_->setWindowFlag(Qt::WindowStaysOnTopHint, Settings().alwaysOnTop());
    client_window_->show();
    client_window_->setTabbedMode(false);
}

//--------------------------------------------------------------------------------------------------
void ClientTab::attachToTab()
{
    if (!client_window_ || !isDetached())
        return;

    client_window_->setParent(this);
    layout()->addWidget(client_window_);
    client_window_->show();
    client_window_->setTabbedMode(true);
}

//--------------------------------------------------------------------------------------------------
QWidget* ClientTab::detachedWindow() const
{
    return isDetached() ? client_window_.get() : nullptr;
}

//--------------------------------------------------------------------------------------------------
QByteArray ClientTab::saveState()
{
    return client_window_ ? client_window_->saveState() : QByteArray();
}

//--------------------------------------------------------------------------------------------------
void ClientTab::restoreState(const QByteArray& state)
{
    if (client_window_)
        client_window_->restoreState(state);
}

//--------------------------------------------------------------------------------------------------
void ClientTab::activate(QStatusBar* /* statusbar */)
{
    if (!client_window_)
        return;

    scheduleRepaint();
}

//--------------------------------------------------------------------------------------------------
void ClientTab::deactivate(QStatusBar* /* statusbar */)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::hasStatusBar() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
QList<Tab::ActionGroupEntry> ClientTab::actionGroups() const
{
    if (!client_window_)
        return {};
    return client_window_->tabActionGroups();
}

//--------------------------------------------------------------------------------------------------
void ClientTab::showEvent(QShowEvent* event)
{
    Tab::showEvent(event);
    scheduleRepaint();
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::eventFilter(QObject* object, QEvent* event)
{
    if (object == client_window_ && event->type() == QEvent::Close && !closing_)
        emit sig_closeRequested();

    return Tab::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void ClientTab::scheduleRepaint()
{
    if (!client_window_)
        return;

    // Force a synchronous repaint of the whole window and all its children so any residual pixels
    // left by previously active widgets (or by the just created session) are cleared.
    // Using QWidget::repaint() bypasses Qt's update batching which sometimes misses native child
    // viewports (header views, etc).
    QPointer<ClientWindow> guarded(client_window_.get());
    QTimer::singleShot(0, guarded, [guarded]()
    {
        if (!guarded)
            return;
        guarded->repaint();
        for (QWidget* child : guarded->findChildren<QWidget*>())
            child->repaint();
    });
}
