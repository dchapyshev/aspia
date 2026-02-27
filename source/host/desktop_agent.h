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

#include "base/desktop/capture_scheduler.h"
#include "base/desktop/screen_capturer.h"

namespace base {
class AudioCapturerWrapper;
class IpcServer;
class ScreenCapturerWrapper;
} // namespace base

namespace common {
class ClipboardMonitor;
} // namespace common

namespace proto::desktop {
class AudioPacket;
class ClipboardEvent;
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

    bool start(const QString& ipc_channel_name);

private slots:
    void onIpcNewConnection();
    void onIpcErrorOccurred();

    void onClientConfigured();
    void onClientFinished();

    void onCaptureScreen();

    void onInjectKeyEvent(const proto::desktop::KeyEvent& event);
    void onInjectTextEvent(const proto::desktop::TextEvent& event);
    void onInjectMouseEvent(const proto::desktop::MouseEvent& event);
    void onInjectTouchEvent(const proto::desktop::TouchEvent& event);
    void onInjectClipboardEvent(const proto::desktop::ClipboardEvent& event);

private slots:
    void onScreenListChanged(
        const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current);
    void onSelectScreen(const proto::desktop::Screen& screen);
    void onCursorPositionChanged(const QPoint& position);
    void onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name);
    void onClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void onAudioCaptured(const proto::desktop::AudioPacket& packet);

private:
    void mergeClientConfigurations();

    base::IpcServer* ipc_server_ = nullptr;
    QList<DesktopAgentClient*> clients_;

    common::ClipboardMonitor* clipboard_monitor_ = nullptr;
    InputInjector* input_injector_ = nullptr;
    base::ScreenCapturerWrapper* screen_capturer_ = nullptr;
    base::AudioCapturerWrapper* audio_capturer_ = nullptr;

    QTimer* screen_capture_timer_ = nullptr;
    base::CaptureScheduler capture_scheduler_;

    bool lock_at_disconnect_ = false;
    bool clear_clipboard_ = false;

    Q_DISABLE_COPY_MOVE(DesktopAgent)
};

} // namespace host

#endif // HOST_DESKTOP_AGENT_H
