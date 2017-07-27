//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__PIPE_CHANNEL_H
#define _ASPIA_IPC__PIPE_CHANNEL_H

#include "base/threading/thread.h"
#include "base/macros.h"
#include "protocol/io_buffer.h"

#define ASIO_STANDALONE
#define ASIO_HEADER_ONLY
#define ASIO_HAS_MOVE
#define ASIO_HAS_STD_SYSTEM_ERROR
#define ASIO_HAS_CONSTEXPR
#define ASIO_HAS_STD_ARRAY
#define ASIO_HAS_STD_SHARED_PTR
#define ASIO_HAS_STD_ATOMIC
#define ASIO_HAS_STD_CHRONO
#define ASIO_HAS_STD_FUNCTION
#define ASIO_HAS_STD_ADDRESSOF
#define ASIO_HAS_STD_TYPE_TRAITS
#define ASIO_HAS_CSTDINT
#define ASIO_HAS_STD_THREAD
#define ASIO_WINDOWS
#define ASIO_HAS_GETADDRINFO
#define ASIO_HAS_THREADS

#ifdef NDEBUG
#define ASIO_DISABLE_BUFFER_DEBUGGING
#endif // NDEBUG

#include <asio.hpp>

#include <memory>
#include <queue>

namespace aspia {

class PipeChannelProxy;

class PipeChannel : protected Thread
{
public:
    virtual ~PipeChannel();

    static std::unique_ptr<PipeChannel> CreateServer(std::wstring& channel_id);
    static std::unique_ptr<PipeChannel> CreateClient(const std::wstring& channel_id);

    using DisconnectHandler = std::function<void()>;
    using SendCompleteHandler = std::function<void()>;
    using ReceiveCompleteHandler = std::function<void(IOBuffer)>;

    bool Connect(uint32_t& user_data, DisconnectHandler disconnect_handler = nullptr);

    std::shared_ptr<PipeChannelProxy> pipe_channel_proxy() const
    {
        return proxy_;
    }

private:
    friend class PipeChannelProxy;

    enum class Mode { SERVER, CLIENT };

    PipeChannel(HANDLE pipe, Mode mode);

    void Send(IOBuffer buffer, SendCompleteHandler handler);
    void Send(IOBuffer buffer);
    void Receive(ReceiveCompleteHandler handler);

    void ScheduleWrite();
    void OnWriteSizeComplete(const std::error_code& code, size_t bytes_transferred);
    void OnWriteComplete(const std::error_code& code, size_t bytes_transferred);
    void ScheduleRead();
    void OnReadSizeComplete(const std::error_code& code, size_t bytes_transferred);
    void OnReadComplete(const std::error_code& code, size_t bytes_transferred);
    void DoDisconnect();
    void Run() override;

    const Mode mode_;
    DisconnectHandler disconnect_handler_;

    asio::io_service io_service_;
    std::unique_ptr<asio::io_service::work> work_;
    asio::windows::stream_handle stream_{ io_service_ };

    std::queue<std::pair<IOBuffer, SendCompleteHandler>> write_queue_;
    std::mutex write_queue_lock_;

    IOBuffer write_buffer_;
    uint32_t write_size_ = 0;

    ReceiveCompleteHandler receive_complete_handler_;

    IOBuffer read_buffer_;
    uint32_t read_size_ = 0;

    std::shared_ptr<PipeChannelProxy> proxy_;

    DISALLOW_COPY_AND_ASSIGN(PipeChannel);
};

} // namespace aspia

#endif // _ASPIA_IPC__PIPE_CHANNEL_H
