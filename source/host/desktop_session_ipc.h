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

#ifndef HOST_DESKTOP_SESSION_IPC_H
#define HOST_DESKTOP_SESSION_IPC_H

#include <QMap>

#include "base/serialization.h"
#include "base/shared_pointer.h"
#include "base/desktop/shared_memory_frame.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/shared_memory.h"
#include "host/desktop_session.h"

namespace host {

class DesktopSessionIpc final : public DesktopSession
{
    Q_OBJECT

public:
    DesktopSessionIpc(base::IpcChannel* ipc_channel, QObject* parent = nullptr);
    ~DesktopSessionIpc() final;

    // DesktopSession implementation.
    void start() final;
    void control(proto::internal::DesktopControl::Action action) final;
    void configure(const Config& config) final;
    void selectScreen(const proto::desktop::Screen& screen) final;
    void captureScreen() final;
    void setScreenCaptureFps(int fps) final;
    void injectKeyEvent(const proto::desktop::KeyEvent& event) final;
    void injectTextEvent(const proto::desktop::TextEvent& event) final;
    void injectMouseEvent(const proto::desktop::MouseEvent& event) final;
    void injectTouchEvent(const proto::desktop::TouchEvent& event) final;
    void injectClipboardEvent(const proto::desktop::ClipboardEvent& event) final;

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);

private:
    using SharedBuffers = QMap<int, base::SharedPointer<base::SharedMemory>>;

    void onScreenCaptured(const proto::internal::ScreenCaptured& screen_captured);
    void onCreateSharedBuffer(int shared_buffer_id);
    void onReleaseSharedBuffer(int shared_buffer_id);
    base::SharedPointer<base::SharedMemory> sharedBuffer(int shared_buffer_id);

    base::SessionId session_id_ = base::kInvalidSessionId;
    base::IpcChannel* ipc_channel_ = nullptr;
    SharedBuffers shared_buffers_;
    base::SharedMemoryFrame last_frame_;
    base::MouseCursor last_mouse_cursor_;
    std::unique_ptr<proto::desktop::ScreenList> last_screen_list_;

    std::chrono::milliseconds update_interval_ { 40 }; // 25 fps by default.

    base::Serializer<proto::internal::ServiceToDesktop> outgoing_message_;
    base::Parser<proto::internal::DesktopToService> incoming_message_;

    Q_DISABLE_COPY(DesktopSessionIpc)
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_IPC_H
