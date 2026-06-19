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

#ifndef CLIENT_ANDROID_DESKTOP_WINDOW_H
#define CLIENT_ANDROID_DESKTOP_WINDOW_H

#include <QPointer>
#include <QSize>
#include <QString>
#include <QWidget>

#include <memory>

#include "base/desktop/shared_frame.h"
#include "base/scoped_qpointer.h"
#include "client/client.h"
#include "client/config.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"

namespace proto::control {
class Capabilities;
} // namespace proto::control

namespace proto::cursor {
class Position;
} // namespace proto::cursor

class BottomSheet;
class ClientDesktop;
class DesktopView;
class FloatingActionButton;
class KeyBar;
class Label;
class Router;
class SessionState;
class StatisticsDialog;

class DesktopWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopWindow(const HostConfig& host, QWidget* parent = nullptr);
    ~DesktopWindow() final;

signals:
    void sig_closed();
    void sig_screenSelected(const proto::screen::Screen& screen);
    void sig_powerControl(proto::power::Control::Action action);

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onStatusChanged(Client::Status status, const QVariant& data);
    void onFrameChanged(const QSize& screen_size, SharedFrame frame);
    void onScreenListChanged(const proto::screen::ScreenList& screen_list);
    void onCapabilitiesChanged(const proto::control::Capabilities& capabilities);
    void onCursorPositionChanged(const proto::cursor::Position& position);
    void onShowActions();
    void onShowStatistics();
    void onKeyboardInsetChanged(int inset);
    void onApplicationStateChanged(Qt::ApplicationState state);

private:
    void start();
    void fetchConnectionOffer();
    void requestConnectionOffer(Router* router);
    void startNewClient();
    void reconnect();
    void showPowerActions();
    void triggerPowerAction(proto::power::Control::Action action, const QString& confirm_text);
    void setStatusText(const QString& text);

    HostConfig host_;
    std::shared_ptr<SessionState> session_state_;
    ScopedQPointer<ClientDesktop> client_;

    proto::screen::ScreenList screen_list_;
    QPointer<BottomSheet> action_sheet_;
    QPointer<StatisticsDialog> statistics_dialog_;

    DesktopView* view_ = nullptr;
    Label* status_ = nullptr;
    FloatingActionButton* fab_ = nullptr;
    KeyBar* key_bar_ = nullptr;
    bool connected_ = false;

    // True once a session has been established at least once, so a connection lost in the background
    // is auto-reconnected when the app returns to the foreground.
    bool was_connected_ = false;
    bool host_is_windows_ = false;
    bool power_control_available_ = false;

    Q_DISABLE_COPY_MOVE(DesktopWindow)
};

#endif // CLIENT_ANDROID_DESKTOP_WINDOW_H
