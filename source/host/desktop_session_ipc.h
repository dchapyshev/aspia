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

#include "base/ipc/ipc_channel.h"
#include "host/desktop_session.h"

namespace host {

class DesktopSessionIpc
    : public DesktopSession,
      public base::IpcChannel::Listener
{
public:
    DesktopSessionIpc(std::unique_ptr<base::IpcChannel> channel, Delegate* delegate);
    ~DesktopSessionIpc();

    // DesktopSession implementation.
    void start() override;
    void stop() override;
    void control(proto::internal::Control::Action action) override;
    void configure(const Config& config) override;
    void selectScreen(const proto::Screen& screen) override;
    void captureScreen() override;
    void injectKeyEvent(const proto::KeyEvent& event) override;
    void injectMouseEvent(const proto::MouseEvent& event) override;
    void injectClipboardEvent(const proto::ClipboardEvent& event) override;

protected:
    // base::IpcChannel::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    class SharedBuffer;
    using SharedBuffers = std::map<int, std::unique_ptr<SharedBuffer>>;

    void onScreenCaptured(const proto::internal::ScreenCaptured& screen_captured);
    void onAudioCaptured(const proto::AudioPacket& audio_packet);
    void onCreateSharedBuffer(int shared_buffer_id);
    void onReleaseSharedBuffer(int shared_buffer_id);
    std::unique_ptr<SharedBuffer> sharedBuffer(int shared_buffer_id);

    std::unique_ptr<base::IpcChannel> channel_;
    SharedBuffers shared_buffers_;
    std::unique_ptr<base::Frame> last_frame_;
    std::unique_ptr<base::MouseCursor> last_mouse_cursor_;
    std::unique_ptr<proto::ScreenList> last_screen_list_;

    proto::internal::ServiceToDesktop outgoing_message_;
    proto::internal::DesktopToService incoming_message_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionIpc);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_IPC_H
