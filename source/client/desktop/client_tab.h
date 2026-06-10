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

#ifndef CLIENT_DESKTOP_CLIENT_TAB_H
#define CLIENT_DESKTOP_CLIENT_TAB_H

#include "base/scoped_qpointer.h"
#include "client/desktop/tab.h"

class QStatusBar;
class ClientWindow;

class ClientTab final : public Tab
{
    Q_OBJECT

public:
    explicit ClientTab(ClientWindow* client_window, QWidget* parent = nullptr);
    ~ClientTab() final;

    ClientWindow* clientWindow() const;

    // Tab implementation.
    bool isDetachable() const final;
    bool isDetached() const final;
    void detachToWindow() final;
    void attachToTab() final;
    QWidget* detachedWindow() const final;
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;
    bool hasStatusBar() const final;
    QList<ActionGroupEntry> actionGroups() const final;

protected:
    // QWidget implementation.
    void showEvent(QShowEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private:
    void scheduleRepaint();

    ScopedQPointer<ClientWindow> client_window_;
    bool closing_ = false;

    Q_DISABLE_COPY_MOVE(ClientTab)
};

#endif // CLIENT_DESKTOP_CLIENT_TAB_H
