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
#include <QTimer>

#include <sys/types.h>

#include <memory>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "base/desktop/power_save_blocker.h"
#include "host/capture_scheduler.h"
#include "host/screen_capturer.h"
#include "proto/desktop_audio.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class AudioCapturerWrapper;
class AudioEncoder;
class CursorEncoder;
class DesktopAgentClient;
class DesktopEnvironment;
class DesktopResizer;
class InputInjector;
class IpcChannel;
class ScaleReducer;
class VideoEncoder;

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

    void onInjectMouseEvent(const proto::input::MouseEvent& event);
    void onInjectKeyEvent(const proto::input::KeyEvent& event);
    void onInjectTextEvent(const proto::input::TextEvent& event);
    void onInjectTouchEvent(const proto::input::TouchEvent& event);

    void onSelectScreen(const proto::screen::Screen& screen);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onScreenListChanged(
        const ScreenCapturer::ScreenList& list, ScreenCapturer::ScreenId current);
    void onScreenTypeChanged(ScreenCapturer::ScreenType type, const QString& name);
    void onPreferredSizeChanged();
    void onKeyFrameRequested();

    void onCaptureScreen();
    void onOverflowCheck();

#if defined(Q_OS_LINUX)
    // The compositor capture path reports the result of its asynchronous negotiation; on failure the
    // agent falls back to DRM/KMS.
    void onCompositorSourceStarted(bool success);
#endif // defined(Q_OS_LINUX)

private:
    void startClient(const QString& ipc_channel_name);
    void selectCapturer(ScreenCapturer::Error last_error);
#if defined(Q_OS_LINUX)
    // Chooses the Linux capture path: X11, or on Wayland a compositor source / KWin / wlr on a user
    // session, otherwise DRM/KMS + uinput.
    void setupLinuxCapture();
    // Switches to DRM/KMS + uinput after the compositor capture path failed to start.
    void fallbackToKms();
    // Sends |text| (UTF-8) to the connected clients as a clipboard event (a finished terminal selection).
    void sendClipboardText(const std::string& text);
#endif // defined(Q_OS_LINUX)
    ScreenCapturer::ScreenId defaultScreen();
    void selectScreen(ScreenCapturer::ScreenId screen_id, const QSize& resolution);
    void sendCurrentScreenList();
    void encodeScreen(const Frame* frame);
    void encodeCursor(const MouseCursor* cursor);
    void encodeAudio(const proto::audio::Packet& packet);
    void createVideoEncoder();

    // Control channel between service and agent.
    IpcChannel* ipc_channel_ = nullptr;

    QList<DesktopAgentClient*> clients_;

    PowerSaveBlocker power_save_blocker_;
    InputInjector* input_injector_ = nullptr;
    ScopedQPointer<ScreenCapturer> screen_capturer_;

#if defined(Q_OS_LINUX)
    // X11: X11 grabber + injector. KMS: capture below the compositor via DRM/KMS + uinput (login
    // screen, or no usable compositor screen-cast). COMPOSITOR: a PipeWire stream from a compositor
    // source (Mutter ScreenCast or the xdg-desktop-portal session, picked inside the capturer) with its
    // own input. KWIN: KDE KWin ScreenShot2 polling + uinput. WLR: wlroots zwlr_screencopy + uinput.
    enum class CaptureMode { X11, KMS, COMPOSITOR, KWIN, WLR, VT };
    CaptureMode capture_mode_ = CaptureMode::KMS;

    // Active session user's uid (for COMPOSITOR/KWIN/WLR: reaching the session bus as that user).
    uid_t session_uid_ = 0;
#endif // defined(Q_OS_LINUX)

    ScopedQPointer<AudioCapturerWrapper> audio_capturer_;
    DesktopEnvironment* desktop_environment_ = nullptr;
    std::unique_ptr<DesktopResizer> screen_resizer_;

    ScreenCapturer::Type preferred_capturer_  = ScreenCapturer::Type::DEFAULT;
    ScreenCapturer::ScreenId last_screen_id_ = ScreenCapturer::kInvalidScreenId;

    bool vp8_supported_ = false;
    bool vp9_supported_ = false;
    bool h264_supported_ = false;
    bool h264_enabled_ = false;

    proto::video::Encoding video_encoding_ = proto::video::ENCODING_VP8;
    qint64 last_bandwidth_ = 0;

    int screen_count_ = 0;
    QPoint last_cursor_pos_;
    QSize source_size_;
    QSize preferred_size_;
    QSize preferred_resolution_;
    quint64 frame_count_ = 0;

    QTimer* capture_timer_ = nullptr;
    CaptureScheduler capture_scheduler_;

    std::unique_ptr<ScaleReducer> scale_reducer_;
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<CursorEncoder> cursor_encoder_;
    std::unique_ptr<AudioEncoder> audio_encoder_;

    Serializer<proto::screen::HostToClient,
               proto::cursor::HostToClient,
               proto::video::HostToClient,
               proto::audio::HostToClient,
               proto::clipboard::HostToClient> outgoing_message_;

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

#endif // HOST_DESKTOP_AGENT_H
