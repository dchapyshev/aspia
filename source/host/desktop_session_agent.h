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

#include "base/desktop/screen_capturer_wrapper.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/shared_memory_factory.h"
#include "base/threading/thread.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_internal.pb.h"

#include <QPointer>
#include <QTimer>

namespace base {

class AudioCapturerWrapper;
class CaptureScheduler;
class MessageWindow;
class TaskRunner;
class Thread;
class SharedFrame;

} // namespace base

namespace host {

class InputInjector;

class DesktopSessionAgent final
    : public QObject,
      public base::SharedMemoryFactory::Delegate,
      public base::ScreenCapturerWrapper::Delegate,
      public base::Thread::Delegate
{
    Q_OBJECT

public:
    explicit DesktopSessionAgent(QObject* parent = nullptr);
    ~DesktopSessionAgent() final;

    void start(const QString& channel_id);

protected:
    // base::SharedMemoryFactory::Delegate implementation.
    void onSharedMemoryCreate(int id) final;
    void onSharedMemoryDestroy(int id) final;

    // base::ScreenCapturerWrapper::Delegate implementation.
    void onScreenListChanged(const base::ScreenCapturer::ScreenList& list,
                             base::ScreenCapturer::ScreenId current) final;
    void onScreenCaptured(const base::Frame* frame,
                          const base::MouseCursor* mouse_cursor) final;
    void onScreenCaptureError(base::ScreenCapturer::Error error) final;
    void onCursorPositionChanged(const base::Point& position) final;
    void onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name) final;

    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() final;
    void onAfterThreadRunning() final;

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);
    void onClipboardEvent(const proto::ClipboardEvent& event);

private:
    void setEnabled(bool enable);
    void captureBegin();
    void captureEnd(const std::chrono::milliseconds& update_interval);

#if defined(Q_OS_WINDOWS)
    bool onWindowsMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
#endif // defined(Q_OS_WINDOWS)

    bool is_session_enabled_ = false;

#if defined(Q_OS_WINDOWS)
    base::Thread ui_thread_;
    std::unique_ptr<base::MessageWindow> message_window_;
#endif // defined(Q_OS_WINDOWS)

    std::unique_ptr<base::IpcChannel> channel_;
    std::unique_ptr<common::ClipboardMonitor> clipboard_monitor_;
    std::unique_ptr<InputInjector> input_injector_;

    std::unique_ptr<base::SharedMemoryFactory> shared_memory_factory_;
    std::unique_ptr<base::CaptureScheduler> capture_scheduler_;
    std::unique_ptr<base::ScreenCapturerWrapper> screen_capturer_;
    std::unique_ptr<base::AudioCapturerWrapper> audio_capturer_;

    QPointer<QTimer> screen_capture_timer_ = nullptr;

    base::ScreenCapturer::Type preferred_video_capturer_ = base::ScreenCapturer::Type::DEFAULT;
    bool lock_at_disconnect_ = false;
    bool clear_clipboard_ = false;

    proto::internal::ServiceToDesktop incoming_message_;
    proto::internal::DesktopToService outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_AGENT_H
