//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__UI__HOST_NOTIFIER_WINDOW_H
#define HOST__UI__HOST_NOTIFIER_WINDOW_H

#include "base/macros_magic.h"
#include "proto/notifier.pb.h"
#include "ui_host_notifier_window.h"

namespace host {

class HostNotifierWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HostNotifierWindow(QWidget* parent = nullptr);
    ~HostNotifierWindow() = default;

public slots:
    void onConnectEvent(const proto::notifier::ConnectEvent& event);
    void onDisconnectEvent(const proto::notifier::DisconnectEvent& event);

signals:
    void killSession(const std::string& uuid);

protected:
    // QWidget implementation.
    bool eventFilter(QObject* object, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onShowHidePressed();
    void onDisconnectAllPressed();
    void onContextMenu(const QPoint& point);
    void updateWindowPosition();

private:
    void showNotifier();
    void hideNotifier();

    Ui::HostNotifierWindow ui;

    QPoint start_pos_;
    QRect window_rect_;

    DISALLOW_COPY_AND_ASSIGN(HostNotifierWindow);
};

} // namespace host

#endif // HOST__UI__HOST_NOTIFIER_WINDOW_H
