//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_DESKTOP_SESSION_AGENT_H
#define HOST_DESKTOP_SESSION_AGENT_H

#include <QPointer>
#include <QTimer>

#include "base/serialization.h"
#include "base/thread.h"
#include "base/desktop/capture_scheduler.h"
#include "base/desktop/screen_capturer_wrapper.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/shared_memory_factory.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_internal.pb.h"

namespace base {

class AudioCapturerWrapper;
class MessageWindow;
class Thread;

} // namespace base

namespace host {

class InputInjector;

class DesktopSessionAgent final : public QObject
{
    Q_OBJECT

public:
    explicit DesktopSessionAgent(QObject* parent = nullptr);
    ~DesktopSessionAgent() final;

    void start(const QString& channel_id);

private slots:
    void onBeforeThreadRunning();
    void onAfterThreadRunning();
    void onSharedMemoryCreate(int id);
    void onSharedMemoryDestroy(int id);
    void onScreenListChanged(
        const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current);
    void onCursorPositionChanged(const base::Point& position);
    void onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name);
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);
    void onClipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void setEnabled(bool enable);
    void captureScreen();
    void scheduleNextCapture(const std::chrono::milliseconds& update_interval);

#if defined(Q_OS_WINDOWS)
    bool onWindowsMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
#endif // defined(Q_OS_WINDOWS)

    bool is_session_enabled_ = false;

#if defined(Q_OS_WINDOWS)
    base::Thread ui_thread_;
    std::unique_ptr<base::MessageWindow> message_window_;
#endif // defined(Q_OS_WINDOWS)

    base::IpcChannel* ipc_channel_ = nullptr;

    QPointer<common::ClipboardMonitor> clipboard_monitor_;
    QPointer<InputInjector> input_injector_;
    QPointer<base::SharedMemoryFactory> shared_memory_factory_;
    QPointer<base::ScreenCapturerWrapper> screen_capturer_;
    QPointer<base::AudioCapturerWrapper> audio_capturer_;

    QTimer* screen_capture_timer_ = nullptr;
    base::CaptureScheduler capture_scheduler_;

    base::ScreenCapturer::Type preferred_video_capturer_ = base::ScreenCapturer::Type::DEFAULT;
    bool lock_at_disconnect_ = false;
    bool clear_clipboard_ = false;

    base::Parser<proto::internal::ServiceToDesktop> incoming_message_;
    base::Serializer<proto::internal::DesktopToService> outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_AGENT_H
