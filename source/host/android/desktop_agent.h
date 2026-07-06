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

#include <QList>
#include <QObject>
#include <QSize>

#include <memory>

#include "base/scoped_qpointer.h"

namespace proto::input {
class KeyEvent;
class MouseEvent;
class TextEvent;
class TouchEvent;
} // namespace proto::input

namespace proto::video {
enum Encoding : int;
} // namespace proto::video

class DesktopClient;
class Frame;
class InputInjector;
class QTimer;
class ScaleReducer;
class ScreenCapturer;
class VideoEncoder;

// Shared desktop capture/encode engine for the Android host, the in-process analog of the desktop
// DesktopAgent. A single instance (owned by Server) serves every connected desktop client: it owns the
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

private:
    void createVideoEncoder();
    bool startCapturer();
    void sendScreenList();
    void encodeScreen(const Frame* frame);

    QTimer* capture_timer_ = nullptr;
    ScopedQPointer<ScreenCapturer> screen_capturer_;
    ScopedQPointer<InputInjector> input_injector_;
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<ScaleReducer> scale_reducer_;

    // Connected desktop clients (owned by Server). Encoded frames are broadcast to all of them.
    QList<DesktopClient*> clients_;

    proto::video::Encoding video_encoding_;

    QSize source_size_;
    QSize preferred_size_;
    quint64 frame_count_ = 0;

    Q_DISABLE_COPY_MOVE(DesktopAgent)
};

#endif // HOST_ANDROID_DESKTOP_AGENT_H
