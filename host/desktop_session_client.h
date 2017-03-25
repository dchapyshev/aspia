//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_client.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__DESKTOP_SESSION_CLIENT_H
#define _ASPIA_HOST__DESKTOP_SESSION_CLIENT_H

#include <memory>

#include "base/scoped_aligned_buffer.h"
#include "base/lock.h"
#include "ipc/pipe_client_channel.h"
#include "host/input_injector.h"
#include "host/screen_sender.h"
#include "protocol/clipboard.h"

namespace aspia {

class DesktopSessionClient
{
public:
    DesktopSessionClient();
    ~DesktopSessionClient() = default;

    void Execute(const std::wstring& input_channel_name,
                 const std::wstring& output_channel_name);

private:
    bool OnIncommingMessage(const uint8_t* buffer, uint32_t size);

    bool WriteMessage(const proto::desktop::HostToClient& message);

    bool ReadPointerEvent(const proto::PointerEvent& event);
    bool ReadKeyEvent(const proto::KeyEvent& event);
    bool ReadClipboardEvent(const proto::ClipboardEvent& event);
    bool ReadPowerEvent(const proto::PowerEvent& event);
    bool ReadConfig(const proto::DesktopConfig& config);

    bool SendClipboardEvent(const proto::ClipboardEvent& event);
    bool SendConfigRequest();

private:
    PipeClientChannel ipc_channel_;

    ScopedAlignedBuffer outgoing_buffer_;
    Lock outgoing_lock_;

    ScreenSender screen_sender_;
    std::unique_ptr<InputInjector> input_injector_;
    Clipboard clipboard_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionClient);
};

} // namespace aspia

#endif // _ASPIA_HOST__DESKTOP_SESSION_CLIENT_H
