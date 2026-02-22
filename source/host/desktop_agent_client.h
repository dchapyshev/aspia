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
#include <QPointer>

#include <memory>

#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "proto/desktop.h"

#if defined(Q_OS_WINDOWS)
#include "host/task_manager.h"
#endif // defined(Q_OS_WINDOWS)

namespace base {
class AudioEncoder;
class CursorEncoder;
class Frame;
class MouseCursor;
class ScaleReducer;
class VideoEncoder;
} // namespace base

namespace host {

class DesktopAgentClient final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopAgentClient(base::IpcChannel* ipc_channel, QObject* parent = nullptr);
    ~DesktopAgentClient() final;

    struct Config
    {
        bool disable_font_smoothing = true;
        bool disable_wallpaper = true;
        bool disable_effects = true;
        bool block_input = false;
        bool lock_at_disconnect = false;
        bool clear_clipboard = true;
        bool cursor_position = false;

        bool equals(const Config& other) const
        {
            return (disable_font_smoothing == other.disable_font_smoothing) &&
                   (disable_wallpaper == other.disable_wallpaper) &&
                   (disable_effects == other.disable_effects) &&
                   (block_input == other.block_input) &&
                   (lock_at_disconnect == other.lock_at_disconnect) &&
                   (clear_clipboard == other.clear_clipboard) &&
                   (cursor_position == other.cursor_position);
        }
    };

    proto::peer::SessionType sessionType() const;
    const Config& config() const;

    void onScreenCaptureData(const base::Frame* frame, const base::MouseCursor* cursor);
    void onScreenCaptureError(proto::desktop::VideoErrorCode error_code);
    void onScreenListChanged(const proto::desktop::ScreenList& screen_list);
    void onScreenTypeChanged(const proto::desktop::ScreenType& screen_type);
    void onCursorPositionChanged(const QPoint& cursor_position);
    void onClipbaordEvent(const proto::desktop::ClipboardEvent& event);
    void onAudioCaptureData(const proto::desktop::AudioPacket& audio_packet);

public slots:
    void start();

signals:
    void sig_injectKeyEvent(const proto::desktop::KeyEvent& event);
    void sig_injectTextEvent(const proto::desktop::TextEvent& event);
    void sig_injectMouseEvent(const proto::desktop::MouseEvent& event);
    void sig_injectTouchEvent(const proto::desktop::TouchEvent& event);
    void sig_injectClipboardEvent(const proto::desktop::ClipboardEvent& event);

    void sig_captureScreen();
    void sig_selectScreen(const proto::desktop::Screen& screen);

    void sig_configured();
    void sig_finished();

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer);

#if defined(Q_OS_WINDOWS)
    void onTaskManagerMessage(const proto::task_manager::HostToClient& message);
    void onUpdateSessionsList();
#endif // defined(Q_OS_WINDOWS)

private:
    void readSessionMessage(quint8 channel_id, const QByteArray& buffer);
    void sendSessionMessage(const QByteArray& buffer);
    void sendSessionMessage();

    void readMouseEvent(const proto::desktop::MouseEvent& mouse_event);
    void readKeyEvent(const proto::desktop::KeyEvent& key_event);
    void readTouchEvent(const proto::desktop::TouchEvent& touch_event);
    void readTextEvent(const proto::desktop::TextEvent& text_event);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event);
    void readExtension(const proto::desktop::Extension& extension);
    void readConfig(const proto::desktop::Config& config);
    void readSelectScreenExtension(const std::string& data);
    void readPreferredSizeExtension(const std::string& data);
    void readVideoPauseExtension(const std::string& data);
    void readAudioPauseExtension(const std::string& data);
    void readPowerControlExtension(const std::string& data);
    void readRemoteUpdateExtension(const std::string& data);
    void readSystemInfoExtension(const std::string& data);
    void readVideoRecordingExtension(const std::string& data);
    void readTaskManagerExtension(const std::string& data);
    void readSwitchSessionExtension(const std::string& data);

    void sendCapabilities();

    base::IpcChannel* ipc_channel_ = nullptr;

    proto::peer::SessionType session_type_ = proto::peer::SESSION_TYPE_UNKNOWN;
    Config config_;

    std::unique_ptr<base::ScaleReducer> scale_reducer_;
    std::unique_ptr<base::VideoEncoder> video_encoder_;
    std::unique_ptr<base::CursorEncoder> cursor_encoder_;
    std::unique_ptr<base::AudioEncoder> audio_encoder_;

    QSize source_size_;
    QSize preferred_size_;
    QSize forced_size_;

    bool is_video_paused_ = false;
    bool is_audio_paused_ = false;

#if defined(Q_OS_WINDOWS)
    QPointer<TaskManager> task_manager_;
#endif // defined(Q_OS_WINDOWS)

    base::Parser<proto::desktop::ClientToHost> incoming_message_;
    base::Serializer<proto::desktop::HostToClient> outgoing_message_;

    Q_DISABLE_COPY_MOVE(DesktopAgentClient)
};

} // namespace host

#endif // HOST_DESKTOP_AGENT_CLIENT_H
