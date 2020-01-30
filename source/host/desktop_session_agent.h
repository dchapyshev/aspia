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

#include "base/macros_magic.h"
#include "desktop/screen_capturer_wrapper.h"
#include "ipc/ipc_listener.h"
#include "ipc/shared_memory_factory.h"
#include "proto/desktop_internal.pb.h"

#include <memory>
#include <string>

namespace base {
class Thread;
} // namespace base

namespace desktop {
class SharedFrame;
} // namespace desktop

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class InputInjector;

class DesktopSessionAgent
    : public std::enable_shared_from_this<DesktopSessionAgent>,
      public ipc::Listener,
      public ipc::SharedMemoryFactory::Delegate,
      public desktop::ScreenCapturerWrapper::Delegate
{
public:
    DesktopSessionAgent();
    ~DesktopSessionAgent();

    void start(std::u16string_view channel_id);

protected:
    // ipc::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

    // ipc::SharedMemoryFactory::Delegate implementation.
    void onSharedMemoryCreate(int id) override;
    void onSharedMemoryDestroy(int id) override;

    // desktop::ScreenCapturerWrapper::Delegate implementation.
    void onScreenListChanged(const desktop::ScreenCapturer::ScreenList& list,
                             desktop::ScreenCapturer::ScreenId current) override;
    void onScreenCaptured(std::unique_ptr<desktop::SharedFrame> frame) override;

private:
    std::unique_ptr<ipc::Channel> channel_;
    proto::internal::ServiceToDesktop incoming_message_;
    proto::internal::DesktopToService outgoing_message_;

    std::unique_ptr<base::Thread> clipboard_thread_;
    std::unique_ptr<InputInjector> input_injector_;

    std::unique_ptr<ipc::SharedMemoryFactory> shared_memory_factory_;
    std::unique_ptr<desktop::ScreenCapturerWrapper> screen_capturer_;
    std::unique_ptr<desktop::SharedFrame> last_frame_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_AGENT_H
