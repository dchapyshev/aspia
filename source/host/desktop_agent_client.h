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

#ifndef HOST_DESKTOP_AGENT_CLIENT_H
#define HOST_DESKTOP_AGENT_CLIENT_H

#include <QObject>

#include "base/logging.h"
#include "proto/desktop_input.h"
#include "proto/desktop_internal.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"
#include "proto/system_info.h"
#include "proto/task_manager.h"

namespace base {
class IpcChannel;
} // namespace base

namespace host {

class TaskManager;

class DesktopAgentClient final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopAgentClient(QObject* parent = nullptr);
    ~DesktopAgentClient() final;

    struct Config
    {
        proto::desktop::VideoEncoding video_encoding = proto::desktop::VIDEO_ENCODING_VP9;
        proto::desktop::AudioEncoding audio_encoding = proto::desktop::AUDIO_ENCODING_UNKNOWN;
        bool disable_wallpaper = true;
        bool disable_effects = true;
        bool block_input = false;
        bool lock_at_disconnect = false;
        bool cursor_position = false;
        bool cursor_shape = false;
    };

    proto::peer::SessionType sessionType() const { return session_type_; }
    proto::desktop::Overflow::State overflowState() const { return overflow_state_; }
    const QSize& preferredSize() const { return preferred_size_; }
    qint64 bandwidth() const { return bandwidth_; }
    const Config& config() const { return config_; }

    void onVideoData(const QByteArray& buffer);
    void onScreenListData(const QByteArray& buffer);
    void onScreenTypeData(const QByteArray& buffer);
    void onCursorPositionData(const QByteArray& buffer);
    void onCursorData(const QByteArray& buffer);
    void onAudioData(const QByteArray& buffer);

public slots:
    void start(const QString& ipc_channel_name);

signals:
    void sig_injectKeyEvent(const proto::desktop::KeyEvent& event);
    void sig_injectTextEvent(const proto::desktop::TextEvent& event);
    void sig_injectMouseEvent(const proto::desktop::MouseEvent& event);
    void sig_injectTouchEvent(const proto::desktop::TouchEvent& event);
    void sig_selectScreen(const proto::desktop::Screen& screen);
    void sig_preferredSizeChanged(const QSize& size);
    void sig_keyFrameRequested();
    void sig_configured();
    void sig_finished();

private slots:
    void onIpcConnected();
    void onIpcErrorOccurred();
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);

    void onTaskManagerMessage(const proto::task_manager::HostToClient& message);

private:
    void readSessionMessage(quint8 channel_id, const QByteArray& buffer);
    void sendSessionMessage(quint8 channel_id, const QByteArray& buffer, bool reliable);

    void readMouseEvent(const proto::desktop::MouseEvent& mouse_event);
    void readKeyEvent(const proto::desktop::KeyEvent& key_event);
    void readTouchEvent(const proto::desktop::TouchEvent& touch_event);
    void readTextEvent(const proto::desktop::TextEvent& text_event);
    void readPreferredSize(const proto::desktop::PreferredSize& size);
    void readVideoPause(const proto::desktop::VideoPause& pause);
    void readAudioPause(const proto::desktop::AudioPause& pause);
    void readConfig(const proto::desktop::Config& config);
    void readPowerControl(const proto::desktop::PowerControl& control);
    void readSystemInfo(const proto::system_info::SystemInfoRequest& request);
    void readTaskManager(const proto::task_manager::ClientToHost& message);
    void readOverflow(proto::desktop::Overflow::State state);
    void sendCapabilities();

    base::IpcChannel* ipc_channel_ = nullptr;

    proto::desktop::Overflow::State overflow_state_ = proto::desktop::Overflow::STATE_NONE;
    proto::peer::SessionType session_type_ = proto::peer::SESSION_TYPE_UNKNOWN;
    Config config_;

    QSize preferred_size_;
    qint64 bandwidth_ = 0;

    bool is_video_paused_ = false;
    bool is_audio_paused_ = false;

#if defined(Q_OS_WINDOWS)
    TaskManager* task_manager_ = nullptr;
#endif // defined(Q_OS_WINDOWS)

    LOG_DECLARE_CONTEXT(DesktopAgentClient);
    Q_DISABLE_COPY_MOVE(DesktopAgentClient)
};

} // namespace host

#endif // HOST_DESKTOP_AGENT_CLIENT_H
