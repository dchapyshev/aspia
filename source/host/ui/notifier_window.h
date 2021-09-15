//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__UI__NOTIFIER_WINDOW_H
#define HOST__UI__NOTIFIER_WINDOW_H

#include "host/user_session_agent.h"
#include "ui_notifier_window.h"

namespace host {

class NotifierWindow : public QWidget
{
    Q_OBJECT

public:
    explicit NotifierWindow(QWidget* parent = nullptr);
    ~NotifierWindow();

public slots:
    void onClientListChanged(const UserSessionAgent::ClientList& clients);
    void disconnectAll();
    void retranslateUi();
    void closeNotifier();

signals:
    void killSession(uint32_t id);
    void finished();

protected:
    // QWidget implementation.
    bool eventFilter(QObject* object, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onShowHidePressed();
    void onContextMenu(const QPoint& point);
    void updateWindowPosition();

private:
    void showNotifier();
    void hideNotifier();
    QRect currentAvailableRect();

    Ui::NotifierWindow ui;

    bool should_be_close_ = false;
    QPoint start_pos_;
    QRect window_rect_;

    DISALLOW_COPY_AND_ASSIGN(NotifierWindow);
};

} // namespace host

#endif // HOST__UI__NOTIFIER_WINDOW_H
