//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_UI_NOTIFIER_WINDOW_H
#define HOST_UI_NOTIFIER_WINDOW_H

#include "host/user_session_agent.h"
#include "ui_notifier_window.h"

namespace host {

class NotifierWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit NotifierWindow(QWidget* parent = nullptr);
    ~NotifierWindow() final;

    QList<quint32> sessions(proto::peer::SessionType session_type);

public slots:
    void onClientListChanged(const host::UserSessionAgent::ClientList& clients);
    void onLockMouse(bool value);
    void onLockKeyboard(bool value);
    void onPause(bool value);
    void onStop();
    void retranslateUi();
    void closeNotifier();

signals:
    void sig_killSession(quint32 id);
    void sig_lockMouse(bool enable);
    void sig_lockKeyboard(bool enable);
    void sig_pause(bool enable);
    void sig_finished();

protected:
    // QWidget implementation.
    bool eventFilter(QObject* object, QEvent* event) final;
    void hideEvent(QHideEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    void moveEvent(QMoveEvent* event) final;
    void paintEvent(QPaintEvent* event) final;

private slots:
    void onUpdateWindowPosition();
    void onShowNotifier();
    void onHideNotifier();
    void onThemeChanged();

private:
    QRect currentAvailableRect();

    Ui::NotifierWindow ui;

    bool should_be_close_ = false;
    QPoint start_pos_;
    QRect window_rect_;

    Q_DISABLE_COPY(NotifierWindow)
};

} // namespace host

#endif // HOST_UI_NOTIFIER_WINDOW_H
