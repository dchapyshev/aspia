//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_server.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__DESKTOP_SESSION_SERVER_H
#define _ASPIA_HOST__DESKTOP_SESSION_SERVER_H

#include <stdint.h>
#include <functional>
#include <memory>

#include "base/scoped_aligned_buffer.h"
#include "base/object_watcher.h"
#include "base/waitable_timer.h"
#include "base/macros.h"
#include "base/thread.h"
#include "base/process.h"
#include "base/lock.h"
#include "ipc/pipe_server_channel.h"
#include "host/console_session_watcher.h"

namespace aspia {

class DesktopSessionServer : private ConsoleSessionWatcher
{
public:
    DesktopSessionServer();
    ~DesktopSessionServer() = default;

    typedef std::function<bool(const uint8_t*, uint32_t)> OutgoingMessageCallback;
    typedef std::function<void()> ErrorCallback;

    void StartSession(const OutgoingMessageCallback& outgoing_message_callback,
                      const ErrorCallback& error_callback);

    void StopSession();

    void OnMessageFromClient(const uint8_t* buffer, uint32_t size);

private:
    void OnSessionAttached(uint32_t session_id) override;
    void OnSessionDetached() override;
    void OnHostProcessClose();

private:
    enum class State { Detached, Attached };

    WaitableTimer timer_;

    State state_;

    Process process_;
    ObjectWatcher process_watcher_;

    std::unique_ptr<PipeChannel> ipc_channel_;

    OutgoingMessageCallback outgoing_message_callback_;
    ErrorCallback error_callback_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionServer);
};

} // namespace aspia

#endif // _ASPIA_HOST__DESKTOP_SESSION_SERVER_H
