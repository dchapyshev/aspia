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

#include <chrono>
#include <memory>

#include "base/desktop/shared_frame.h"
#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "client/config.h"
#include "client/workers/audio_worker.h"
#include "client/workers/network_worker.h"
#include "client/workers/video_worker.h"
#include "common/clipboard.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_input.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"

namespace proto::control {
class Capabilities;
} // namespace proto::control

class BottomSheet;
class Database;
class DesktopView;
class FloatingActionButton;
class KeyBar;
class Label;
class Router;
class SessionKeeper;
class SessionState;
class StatisticsDialog;
class WorkerManager;

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
    void sig_switchSession(quint32 session_id);

    // Session control fed to the network worker.
    void sig_startConnection(std::shared_ptr<SessionState> session_state);
    void sig_sessionReady();
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);

    // Pushes the cursor configuration to the video worker.
    void sig_cursorConfig(bool shape_enabled, bool position_enabled);

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onNetworkConnected();

    // Incoming session channels (video/audio/cursor are parsed by the workers).
    void onScreenMessage(const QByteArray& buffer);
    void onControlMessage(const QByteArray& buffer);
    void onClipboardMessage(const QByteArray& buffer);

    void onFrameChanged(const QSize& screen_size, SharedFrame frame);
    void onCursorPositionChanged(const proto::cursor::Position& position);
    void onVideoH264Disabled();

    void onMouseEvent(const proto::input::MouseEvent& event);
    void onKeyEvent(const proto::input::KeyEvent& event);
    void onTextEvent(const proto::input::TextEvent& event);
    void onCurrentScreenChanged(const proto::screen::Screen& screen);
    void onPowerControl(proto::power::Control::Action action);
    void onSwitchSession(quint32 session_id);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onMetricsRequest();

    void onShowActions();
    void onShowStatistics();
    void onKeyboardInsetChanged(int inset);
    void onApplicationStateChanged(Qt::ApplicationState state);

private:
    void start();
    void fetchConnectionOffer();
    void requestConnectionOffer(Router* router);
    void startNewSession();
    void reconnect();
    void showPowerActions();
    void showSessionActions();
    void triggerPowerAction(proto::power::Control::Action action, const QString& confirm_text);
    void setStatusText(const QString& text);

    void onStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onScreenListChanged(const proto::screen::ScreenList& screen_list);
    void onSessionListChanged(const proto::control::SessionList& session_list);
    void onCapabilitiesChanged(const proto::control::Capabilities& capabilities);

    void sendConfig(const proto::control::Config& config);
    void sendCapabilities();
    void sendSessionListRequest();
    void sendMessage(quint8 channel_id, const QByteArray& buffer);
    void readCapabilities(const proto::control::Capabilities& capabilities);
    void readClipboardEvent(const proto::clipboard::Event& event);

    HostConfig host_;
    std::shared_ptr<SessionState> session_state_;
    ScopedQPointer<Clipboard> clipboard_;

    std::unique_ptr<WorkerManager> worker_manager_;
    QPointer<NetworkWorker> network_worker_;
    QPointer<AudioWorker> audio_worker_;
    QPointer<VideoWorker> video_worker_;
    SessionKeeper* session_keeper_ = nullptr;

    proto::control::Config desktop_config_;
    Serializer<proto::input::ClientToHost> outgoing_message_;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    TimePoint start_time_;

    int read_clipboard_count_ = 0;
    int send_clipboard_count_ = 0;

    bool h264_sw_enabled_ = true;

    proto::screen::ScreenList screen_list_;
    proto::control::SessionList session_list_;
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
