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

#ifndef HOST_ANDROID_HOME_WIDGET_H
#define HOST_ANDROID_HOME_WIDGET_H

#include <QString>

#include "common/android/scroll_area.h"

class IconButton;
class Label;
class Switch;
class QLabel;

// Home section of the Android host: shows the connection credentials (host ID and one-time password)
// and the router connection state, mirroring the desktop host main window. The values are set through
// the public setters; the host agent that supplies them is not wired in yet.
class HomeWidget final : public ScrollArea
{
    Q_OBJECT

public:
    explicit HomeWidget(QWidget* parent = nullptr);
    ~HomeWidget() final;

    enum class RouterState
    {
        DISABLED,
        CONNECTING,
        CONNECTED,
        FAILED
    };

    void setHostId(const QString& host_id);
    void setPassword(const QString& password);
    void setRouterState(RouterState state, const QString& router = QString());

    void retranslate();

signals:
    // The user asked for a new one-time password.
    void sig_newPasswordRequested();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private:
    void updateRouterRow();
    QString routerStatusText() const;

    Label* id_caption_ = nullptr;
    Label* id_value_ = nullptr;
    Label* password_caption_ = nullptr;
    Label* password_value_ = nullptr;
    IconButton* new_password_button_ = nullptr;
    Label* access_caption_ = nullptr;
    Switch* desktop_session_ = nullptr;
    Switch* file_transfer_session_ = nullptr;
    QLabel* router_icon_ = nullptr;
    Label* router_text_ = nullptr;

    QString host_id_ = "-";
    QString password_ = "-";
    RouterState router_state_ = RouterState::DISABLED;
    QString router_;

    Q_DISABLE_COPY_MOVE(HomeWidget)
};

#endif // HOST_ANDROID_HOME_WIDGET_H
