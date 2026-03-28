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

#ifndef HOST_DESKTOP_AGENT_H
#define HOST_DESKTOP_AGENT_H

#include <QObject>

#include "base/serialization.h"
#include "base/desktop/capture_scheduler.h"
#include "base/desktop/power_save_blocker.h"
#include "base/desktop/screen_capturer.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_video.h"

namespace base {
class AudioCapturerWrapper;
class AudioEncoder;
class CursorEncoder;
class DesktopEnvironment;
class DesktopResizer;
class IpcChannel;
class ScaleReducer;
class VideoEncoder;
} // namespace base

namespace proto::desktop {
class AudioPacket;
class KeyEvent;
class MouseEvent;
class Screen;
class TextEvent;
class TouchEvent;
} // namespace proto::desktop

namespace host {

class DesktopAgentClient;
class InputInjector;

class DesktopAgent final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopAgent(QObject* parent = nullptr);
    ~DesktopAgent() final;

    void start(const QString& ipc_channel_name);

private slots:
    void onIpcConnected();
    void onIpcDisconnected();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 ipc_channel_id, const QByteArray& buffer, bool reliable);

    void onClientConfigured();
    void onClientFinished();

    void onInjectMouseEvent(const proto::desktop::MouseEvent& event);
    void onInjectKeyEvent(const proto::desktop::KeyEvent& event);
    void onInjectTextEvent(const proto::desktop::TextEvent& event);
    void onInjectTouchEvent(const proto::desktop::TouchEvent& event);

    void onSelectScreen(const proto::desktop::Screen& screen);
    void onScreenListChanged(
        const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current);
    void onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name);
    void onPreferredSizeChanged();
    void onKeyFrameRequested();

    void onCaptureScreen();
    void onOverflowCheck();

private:
    void startClient(const QString& ipc_channel_name);
    void selectCapturer(base::ScreenCapturer::Error last_error);
    base::ScreenCapturer::ScreenId defaultScreen();
    void selectScreen(base::ScreenCapturer::ScreenId screen_id, const QSize& resolution);
    void encodeScreen(const base::Frame* frame);
    void encodeCursor(const base::MouseCursor* cursor);
    void encodeAudio(const proto::desktop::AudioPacket& packet);

    // Control channel between service and agent.
    base::IpcChannel* ipc_channel_ = nullptr;

    QList<DesktopAgentClient*> clients_;

    base::PowerSaveBlocker power_save_blocker_;
    InputInjector* input_injector_ = nullptr;
    base::ScreenCapturer* screen_capturer_ = nullptr;
    base::AudioCapturerWrapper* audio_capturer_ = nullptr;
    base::DesktopEnvironment* desktop_environment_ = nullptr;
    std::unique_ptr<base::DesktopResizer> screen_resizer_;

    base::ScreenCapturer::Type preferred_capturer_  = base::ScreenCapturer::Type::DEFAULT;
    base::ScreenCapturer::ScreenId last_screen_id_ = base::ScreenCapturer::kInvalidScreenId;

    int screen_count_ = 0;
    QPoint last_cursor_pos_;
    QSize source_size_;
    QSize preferred_size_;
    QSize forced_size_;
    quint64 frame_count_ = 0;

    QTimer* capture_timer_ = nullptr;
    base::CaptureScheduler capture_scheduler_;

    std::unique_ptr<base::ScaleReducer> scale_reducer_;
    std::unique_ptr<base::VideoEncoder> video_encoder_;
    std::unique_ptr<base::CursorEncoder> cursor_encoder_;
    std::unique_ptr<base::AudioEncoder> audio_encoder_;

    base::Serializer<proto::desktop::ScreenData> screen_message_;
    base::Serializer<proto::desktop::VideoData> video_message_;
    base::Serializer<proto::desktop::AudioData> audio_message_;

    bool is_paused_ = false;
    bool is_mouse_locked_ = false;
    bool is_keyboard_locked_ = false;
    bool is_cursor_position_ = false;
    bool is_lock_at_disconnect_ = false;

    QTimer* overflow_timer_ = nullptr;

    int pressure_score_ = 0; // 0..100
    int stable_seconds_ = 0;
    int cooldown_seconds_ = 0;

    const int default_fps_ = 0;
    const int min_fps_ = 0;
    const int max_fps_ = 0;

    Q_DISABLE_COPY_MOVE(DesktopAgent)
};

} // namespace host

#endif // HOST_DESKTOP_AGENT_H
