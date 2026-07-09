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

#ifndef HOST_WORKERS_SCREEN_WORKER_H
#define HOST_WORKERS_SCREEN_WORKER_H

#include <QElapsedTimer>
#include <QPoint>
#include <QPointer>
#include <QSize>

#include <sys/types.h>

#include <memory>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "base/threading/worker.h"
#include "host/capture_scheduler.h"
#include "host/screen_capturer.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_control.h"
#include "proto/desktop_cursor.h"
#include "proto/desktop_internal.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class CursorEncoder;
class DesktopEnvironment;
class DesktopResizer;
class InputInjector;
class IpcWorker;
class QTimer;
class ScaleReducer;
class VideoEncoder;

// Runs the video pipeline of the desktop agent: screen capture, capture pacing and video/cursor
// encoding. On Linux the input injector is tied to the selected capture path (it may share
// capturer-owned resources), so it lives here too and InputWorker delegates injection to this
// worker; on Windows and macOS injection is fully handled by InputWorker.
class ScreenWorker final : public Worker
{
    Q_OBJECT

public:
    ScreenWorker();
    ~ScreenWorker() final;

    // Injection entry points used by InputWorker on platforms where the injector is owned by the
    // capture path (Linux). Not connected to signals: InputWorker invokes them in this worker's
    // thread directly. Events arrive already gated and scaled.
    void injectKeyEvent(const proto::input::KeyEvent& event);
    void injectTextEvent(const proto::input::TextEvent& event);
    void injectMouseEvent(const proto::input::MouseEvent& event);
    void injectTouchEvent(const proto::input::TouchEvent& event);
    void setBlockInput(bool enable);

public slots:
    // Applies the configuration merged over all connected clients and (re)starts capturing. Codec
    // flags carry the capability intersection of the clients.
    void onConfigure(const proto::control::Config& config,
                     bool vp8_supported, bool vp9_supported, bool h264_supported);

    // Stops capturing (the last client disconnected).
    void onStopCapture();

    void onSetPaused(bool paused);
    void onSetPreferredSize(const QSize& size);
    void onSelectScreen(const proto::screen::Screen& screen);
    void onClipboardEvent(const proto::clipboard::Event& event);
    void onKeyFrameRequested();

    // Aggregated network feedback from the clients; drives the FPS and encoding selection.
    void onOverflowStateChanged(proto::desktop::Overflow::State state, qint64 bandwidth);

signals:
    // Serialized messages ready to be sent to clients.
    void sig_videoData(const QByteArray& buffer, bool is_key_frame);
    void sig_cursorShapeData(const QByteArray& buffer);
    void sig_cursorPositionData(const QByteArray& buffer);
    void sig_screenListData(const QByteArray& buffer);
    void sig_screenTypeData(const QByteArray& buffer);
    void sig_clipboardData(const QByteArray& buffer);

    // Video pipeline state InputWorker needs for scaling client coordinates and clamping them into
    // the real screen. Emitted only when the values actually change.
    void sig_scaleFactorChanged(double scale_x, double scale_y);

    // Screen geometry plus the active capture backend. On Linux the backend decides which input
    // injector InputWorker creates (X11/uinput), or whether it delegates to this worker (VT/Wayland,
    // where the injector is tied to capturer-owned resources). Emitted only when a value changes.
    void sig_screenInfoChanged(ScreenCapturer::Type type, const QSize& screen_size, const QPoint& offset);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onCaptureScreen();
    void onScreenTypeChanged(ScreenCapturer::ScreenType type, const QString& name);
#if defined(Q_OS_LINUX)
    // The compositor capture path reports the result of its asynchronous negotiation; on failure
    // the worker falls back to DRM/KMS.
    void onCompositorSourceStarted(bool success);
#endif // defined(Q_OS_LINUX)

private:
    void selectCapturer(ScreenCapturer::Error last_error);
#if defined(Q_OS_LINUX)
    // One pass of the Wayland user-session capture chain (compositor screen-cast, wlr, KMS, KWin,
    // portal); sets the capture mode and input injector. Returns false if no path is available yet.
    bool probeUserWaylandCapture(uid_t uid);
    // Chooses the Linux capture path: X11, or on Wayland a compositor source / KWin / wlr on a user
    // session, otherwise DRM/KMS + uinput.
    void setupLinuxCapture();
    // Re-runs the capture path selection. A backend committed while the session was still settling
    // (login/logout transition) can pass its trial capture and then fail on every real frame.
    void reselectLinuxCapture();
    // Called on every failed capture attempt: once no frame has been produced for a while, re-runs
    // the capture path selection against the settled session.
    void registerCaptureFailure();
    // Switches to DRM/KMS + uinput after the compositor capture path failed to start.
    void fallbackToKms();
    // Sends |text| (UTF-8) to the connected clients as a clipboard event (a finished terminal selection).
    void sendClipboardText(const std::string& text);
#endif // defined(Q_OS_LINUX)
    ScreenCapturer::ScreenId defaultScreen();
    void selectScreen(ScreenCapturer::ScreenId screen_id, const QSize& resolution);
    void sendCurrentScreenList();
    void sendScreenList(const ScreenCapturer::ScreenList& list, ScreenCapturer::ScreenId current);
    void encodeScreen(const Frame* frame);
    void encodeCursor(const MouseCursor* cursor);
    void createVideoEncoder();
    void updateInjectorScreenInfo(const Frame* frame);

    // Source of the client requests and control commands. Resolved through WorkerManager on start.
    QPointer<IpcWorker> ipc_worker_;

    ScopedQPointer<InputInjector> input_injector_;
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

    // Runs while capture attempts fail without a single frame (see registerCaptureFailure()).
    QElapsedTimer capture_error_time_;
#endif // defined(Q_OS_LINUX)

    ScopedQPointer<DesktopEnvironment> desktop_environment_;
    std::unique_ptr<DesktopResizer> screen_resizer_;

    ScreenCapturer::Type preferred_capturer_ = ScreenCapturer::Type::DEFAULT;
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

    ScopedQPointer<QTimer> capture_timer_;
    CaptureScheduler capture_scheduler_;

    std::unique_ptr<ScaleReducer> scale_reducer_;
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<CursorEncoder> cursor_encoder_;

    Serializer<proto::screen::HostToClient,
               proto::cursor::HostToClient,
               proto::video::HostToClient,
               proto::clipboard::HostToClient> serializer_;

    bool is_paused_ = false;
    bool is_cursor_position_ = false;

    // Last values published to InputWorker (sig_scaleFactorChanged / sig_screenInfoChanged).
    double published_scale_x_ = 0.0;
    double published_scale_y_ = 0.0;
    ScreenCapturer::Type published_capture_type_ = ScreenCapturer::Type::DEFAULT;
    QSize published_screen_size_;
    QPoint published_screen_offset_;

    int pressure_score_ = 0; // 0..100
    int stable_seconds_ = 0;
    int cooldown_seconds_ = 0;

    int default_fps_ = 0;
    int min_fps_ = 0;
    int max_fps_ = 0;

    Q_DISABLE_COPY_MOVE(ScreenWorker)
};

#endif // HOST_WORKERS_SCREEN_WORKER_H
