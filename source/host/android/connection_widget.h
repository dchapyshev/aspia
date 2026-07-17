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

#ifndef HOST_ANDROID_CONNECTION_WIDGET_H
#define HOST_ANDROID_CONNECTION_WIDGET_H

#include <QList>
#include <QString>
#include <QWidget>

#include "host/android/server.h"

class Card;
class IconButton;
class Label;
class Switch;
class QLabel;
class QVBoxLayout;

// Home section of the Android host: shows the connection credentials (host ID and one-time password)
// and the router connection state, mirroring the desktop host main window. The values are set through
// the public setters; the host agent that supplies them is not wired in yet.
class ConnectionWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionWidget(QWidget* parent = nullptr);
    ~ConnectionWidget() final;

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
    void setConnectedClients(const QList<Server::ClientInfo>& clients);

    // The app bar action for this section (shares the host ID and one-time password).
    QList<QWidget*> appBarActions() const;

signals:
    // The user asked for a new one-time password.
    void sig_newPasswordRequested();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onShare();

private:
    void updateRouterRow();
    QString routerStatusText() const;

    Label* id_caption_ = nullptr;
    Label* id_value_ = nullptr;
    Label* password_caption_ = nullptr;
    Label* password_value_ = nullptr;
    IconButton* new_password_button_ = nullptr;
    IconButton* share_button_ = nullptr;
    Label* access_caption_ = nullptr;
    Switch* desktop_session_ = nullptr;
    Switch* file_transfer_session_ = nullptr;
    Card* clients_card_ = nullptr;
    QVBoxLayout* clients_layout_ = nullptr;
    QLabel* router_icon_ = nullptr;
    Label* router_text_ = nullptr;

    // Em dash placeholders until the first credentials update arrives.
    QString host_id_ = QChar(0x2014);
    QString password_ = QChar(0x2014);
    RouterState router_state_ = RouterState::DISABLED;
    QString router_;

    Q_DISABLE_COPY_MOVE(ConnectionWidget)
};

#endif // HOST_ANDROID_CONNECTION_WIDGET_H
