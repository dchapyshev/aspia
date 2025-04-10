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

#ifndef HOST_DESKTOP_SESSION_IPC_H
#define HOST_DESKTOP_SESSION_IPC_H

#include "base/ipc/ipc_channel.h"
#include "base/memory/serializer.h"
#include "host/desktop_session.h"

#include <map>

namespace host {

class DesktopSessionIpc final
    : public DesktopSession,
      public base::IpcChannel::Listener
{
public:
    DesktopSessionIpc(std::unique_ptr<base::IpcChannel> channel, Delegate* delegate);
    ~DesktopSessionIpc() final;

    // DesktopSession implementation.
    void start() final;
    void stop() final;
    void control(proto::internal::DesktopControl::Action action) final;
    void configure(const Config& config) final;
    void selectScreen(const proto::Screen& screen) final;
    void captureScreen() final;
    void setScreenCaptureFps(int fps) final;
    void injectKeyEvent(const proto::KeyEvent& event) final;
    void injectTextEvent(const proto::TextEvent& event) final;
    void injectMouseEvent(const proto::MouseEvent& event) final;
    void injectTouchEvent(const proto::TouchEvent& event) final;
    void injectClipboardEvent(const proto::ClipboardEvent& event) final;

protected:
    // base::IpcChannel::Listener implementation.
    void onIpcDisconnected() final;
    void onIpcMessageReceived(const base::ByteArray& buffer) final;
    void onIpcMessageWritten(base::ByteArray&& buffer) final;

private:
    class SharedBuffer;
    using SharedBuffers = std::map<int, std::unique_ptr<SharedBuffer>>;

    void onScreenCaptured(const proto::internal::ScreenCaptured& screen_captured);
    void onCursorPositionChanged(const proto::CursorPosition& cursor_position);
    void onAudioCaptured(const proto::AudioPacket& audio_packet);
    void onCreateSharedBuffer(int shared_buffer_id);
    void onReleaseSharedBuffer(int shared_buffer_id);
    std::unique_ptr<SharedBuffer> sharedBuffer(int shared_buffer_id);

    base::SessionId session_id_ = base::kInvalidSessionId;
    std::unique_ptr<base::IpcChannel> channel_;
    SharedBuffers shared_buffers_;
    std::unique_ptr<base::Frame> last_frame_;
    std::unique_ptr<base::MouseCursor> last_mouse_cursor_;
    std::unique_ptr<proto::ScreenList> last_screen_list_;
    Delegate* delegate_;

    std::chrono::milliseconds update_interval_ { 40 }; // 25 fps by default.

    base::Serializer serializer_;
    std::unique_ptr<proto::internal::ServiceToDesktop> outgoing_message_;
    std::unique_ptr<proto::internal::DesktopToService> incoming_message_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionIpc);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_IPC_H
