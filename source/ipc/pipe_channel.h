//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CHANNEL_H
#define _ASPIA_IPC__PIPE_CHANNEL_H

#include "base/scoped_object.h"
#include "base/synchronization/waitable_event.h"
#include "base/process/process.h"
#include "base/thread.h"
#include "base/macros.h"
#include "protocol/io_buffer.h"

#include <memory>
#include <string>

namespace aspia {

class PipeChannel : protected Thread
{
public:
    virtual ~PipeChannel();

    static std::unique_ptr<PipeChannel> CreateServer(std::wstring& input_channel_id,
                                                     std::wstring& output_channel_id);

    static std::unique_ptr<PipeChannel> CreateClient(const std::wstring& input_channel_id,
                                                     const std::wstring& output_channel_id);

    class Delegate
    {
    public:
        virtual void OnPipeChannelMessage(const IOBuffer& buffer) = 0;
        virtual void OnPipeChannelConnect(uint32_t user_data) = 0;
        virtual void OnPipeChannelDisconnect() = 0;
    };

    bool Connect(uint32_t user_data, Delegate* delegate);
    void Close();

    void Send(const IOBuffer& buffer);

    void Wait();

private:
    enum class Mode { SERVER, CLIENT };

    PipeChannel(HANDLE read_pipe, HANDLE write_pipe, Mode mode);

    bool Read(void* buf, size_t len);
    bool Write(const void* buf, size_t len);

    void Run() override;

    const Mode mode_;
    Delegate* delegate_ = nullptr;
    uint32_t user_data_ = 0;

    ScopedHandle read_pipe_;
    ScopedHandle write_pipe_;

    WaitableEvent read_event_;
    WaitableEvent write_event_;

    std::mutex read_lock_;
    std::mutex write_lock_;

    DISALLOW_COPY_AND_ASSIGN(PipeChannel);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CHANNEL_H
