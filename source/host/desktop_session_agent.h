//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__DESKTOP_SESSION_AGENT_H
#define HOST__DESKTOP_SESSION_AGENT_H

#include "base/desktop/screen_capturer_wrapper.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/shared_memory_factory.h"
#include "common/clipboard_monitor.h"
#include "proto/desktop_internal.pb.h"

namespace base {
class AudioCapturerWrapper;
class CaptureScheduler;
class TaskRunner;
class Thread;
class SharedFrame;
} // namespace base

namespace host {

class InputInjector;

class DesktopSessionAgent
    : public std::enable_shared_from_this<DesktopSessionAgent>,
      public base::IpcChannel::Listener,
      public base::SharedMemoryFactory::Delegate,
      public base::ScreenCapturerWrapper::Delegate,
      public common::Clipboard::Delegate
{
public:
    explicit DesktopSessionAgent(std::shared_ptr<base::TaskRunner> task_runner);
    ~DesktopSessionAgent();

    void start(std::u16string_view channel_id);

protected:
    // base::IpcChannel::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

    // base::SharedMemoryFactory::Delegate implementation.
    void onSharedMemoryCreate(int id) override;
    void onSharedMemoryDestroy(int id) override;

    // base::ScreenCapturerWrapper::Delegate implementation.
    void onScreenListChanged(const base::ScreenCapturer::ScreenList& list,
                             base::ScreenCapturer::ScreenId current) override;
    void onScreenCaptured(const base::Frame* frame,
                          const base::MouseCursor* mouse_cursor) override;

    // common::Clipboard::Delegate implementation.
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

private:
    void setEnabled(bool enable);
    void captureBegin();
    void captureEnd(const std::chrono::milliseconds& update_interval);

    std::shared_ptr<base::TaskRunner> task_runner_;

    std::unique_ptr<base::IpcChannel> channel_;
    std::unique_ptr<proto::internal::ServiceToDesktop> incoming_message_;
    std::unique_ptr<proto::internal::DesktopToService> outgoing_message_;

    std::unique_ptr<common::ClipboardMonitor> clipboard_monitor_;
    std::unique_ptr<InputInjector> input_injector_;

    std::unique_ptr<base::SharedMemoryFactory> shared_memory_factory_;
    std::unique_ptr<base::CaptureScheduler> capture_scheduler_;
    std::unique_ptr<base::ScreenCapturerWrapper> screen_capturer_;
    std::unique_ptr<base::AudioCapturerWrapper> audio_capturer_;

    base::ScreenCapturer::Type preferred_video_capturer_ = base::ScreenCapturer::Type::DEFAULT;
    bool lock_at_disconnect_ = false;
    bool clear_clipboard_ = false;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_AGENT_H
