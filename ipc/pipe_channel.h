//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CHANNEL_H
#define _ASPIA_IPC__PIPE_CHANNEL_H

#include "aspia_config.h"

#include "base/scoped_aligned_buffer.h"
#include "base/scoped_handle.h"
#include "base/waitable_event.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/macros.h"

#include <functional>

namespace aspia {

class PipeChannel : protected Thread
{
public:
    virtual ~PipeChannel() { }

    typedef std::function<bool(const uint8_t*, uint32_t)> IncommingMessageCallback;

    void Disconnect();

    bool IsValid() const;
    bool WriteMessage(const uint8_t* buffer, uint32_t size);

    void WaitForDisconnect();

protected:
    PipeChannel(HANDLE read_pipe, HANDLE write_pipe, bool is_server);

    typedef struct
    {
        uint32_t pid;
    } PipeHelloMessage;

    static std::wstring CreatePipeName(const std::wstring& channel_id);

    bool Read(void* buf, size_t len);
    bool Write(const void* buf, size_t len);

    IncommingMessageCallback incomming_message_callback_;

    ScopedHandle read_pipe_;
    ScopedHandle write_pipe_;

private:
    void Worker() override;
    void OnStop() override;

    bool is_server_;

    WaitableEvent read_event_;
    WaitableEvent write_event_;

    Lock read_lock_;
    Lock write_lock_;

    ScopedAlignedBuffer incomming_buffer_;

    DISALLOW_COPY_AND_ASSIGN(PipeChannel);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CHANNEL_H
