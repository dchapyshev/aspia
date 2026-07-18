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

#ifndef HOST_ANDROID_DESKTOP_AGENT_H
#define HOST_ANDROID_DESKTOP_AGENT_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSize>

#include <memory>

#include "base/scoped_qpointer.h"
#include "base/serialization.h"
#include "proto/desktop_video.h"

namespace proto::clipboard {
class Event;
} // namespace proto::clipboard

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

class AudioCapturerAndroid;
class FloatingMenuBridge;
class DesktopClient;
class Frame;
class InputInjector;
class QTimer;
class ScaleReducer;
class ScreenCapturer;
class VideoEncoder;

// Shared desktop capture/encode engine for the Android host, the in-process analog of the desktop
// DesktopAgent. A single instance (owned by ServerWorker) serves every connected desktop client: it owns the
// one screen capturer and video encoder, captures and encodes each frame once, and broadcasts the
// encoded video to all clients. The per-connection protocol lives in DesktopClient.
class DesktopAgent final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopAgent(QObject* parent = nullptr);
    ~DesktopAgent() final;

    // Takes ownership of a newly connected desktop client and starts capturing once it is configured.
    // The client is deleted when it finishes.
    void addClient(DesktopClient* client);

private slots:
    void onClientConfigured();
    void onKeyFrameRequested();
    void onPreferredSizeChanged();
    void onScreenListRequested();
    void onClientFinished();
    void onCaptureScreen();

    void onInjectKeyEvent(const proto::input::KeyEvent& event);
    void onInjectTextEvent(const proto::input::TextEvent& event);
    void onInjectMouseEvent(const proto::input::MouseEvent& event);
    void onInjectTouchEvent(const proto::input::TouchEvent& event);

    // Clipboard a client sent; queued for the overlay button to apply on the next tap.
    void onInjectClipboardEvent(const proto::clipboard::Event& event);

    // Device clipboard text read on an overlay tap; broadcast to all clients.
    void onClipboardTextChanged(const QString& text);

    // Encoded audio from the capturer (queued from its Java reader thread); broadcast to all clients.
    void onAudioPacket(const QByteArray& packet);

private:
    void createVideoEncoder();
    bool startCapturer();
    bool isAudioEnabled() const;
    void sendScreenList();
    void encodeScreen(const Frame* frame);

    QTimer* capture_timer_ = nullptr;
    ScopedQPointer<ScreenCapturer> screen_capturer_;
    ScopedQPointer<InputInjector> input_injector_;
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<ScaleReducer> scale_reducer_;

    // Reused message and serialization buffers for the video (and error) packets broadcast every frame.
    Serializer<proto::video::HostToClient> outgoing_message_;

    bool audio_enabled_ = false;
    ScopedQPointer<AudioCapturerAndroid> audio_capturer_;

    // Bridge to the floating clipboard button (the only way to reach the clipboard while backgrounded).
    FloatingMenuBridge* floating_menu_bridge_ = nullptr;

    // Connected desktop clients (owned by ServerWorker). Encoded frames are broadcast to all of them.
    QList<DesktopClient*> clients_;

    proto::video::Encoding video_encoding_;

    // Whether the hardware H264 encoder is usable on this device. Cleared for good when it fails at
    // runtime, so the fallback to VP8 is not overridden by the next client (re)configuration.
    bool h264_enabled_;

    QSize source_size_;
    QSize preferred_size_;
    quint64 frame_count_ = 0;

    Q_DISABLE_COPY_MOVE(DesktopAgent)
};

#endif // HOST_ANDROID_DESKTOP_AGENT_H
