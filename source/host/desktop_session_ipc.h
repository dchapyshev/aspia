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

#ifndef HOST__DESKTOP_SESSION_IPC_H
#define HOST__DESKTOP_SESSION_IPC_H

#include "base/macros_magic.h"
#include "host/desktop_session.h"
#include "ipc/ipc_listener.h"
#include "proto/desktop_internal.pb.h"

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class DesktopSessionIpc
    : public DesktopSession,
      public ipc::Listener
{
public:
    DesktopSessionIpc(std::unique_ptr<ipc::Channel> channel, Delegate* delegate);
    ~DesktopSessionIpc();

    // DesktopSession implementation.
    void start() override;
    void captureScreen() override;
    void selectScreen(const proto::Screen& screen) override;
    void injectKeyEvent(const proto::KeyEvent& event) override;
    void injectPointerEvent(const proto::PointerEvent& event) override;
    void injectClipboardEvent(const proto::ClipboardEvent& event) override;

protected:
    // ipc::Listener implementation.
    void onConnected() override;
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    class SharedBuffer;
    using SharedBuffers = std::map<int, std::unique_ptr<SharedBuffer>>;

    void onCaptureFrameResult(const proto::internal::CaptureFrameResult& result);
    void onCaptureCursorResult(const proto::internal::CaptureCursorResult& result);
    void onCreateSharedBuffer(int shared_buffer_id);
    void onReleaseSharedBuffer(int shared_buffer_id);
    std::unique_ptr<SharedBuffer> sharedBuffer(int shared_buffer_id);

    std::unique_ptr<ipc::Channel> channel_;
    SharedBuffers shared_buffers_;

    proto::internal::ServiceToDesktop outgoing_message_;
    proto::internal::DesktopToService incoming_message_;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionIpc);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_IPC_H
