//
// PROJECT:         Aspia
// FILE:            network/network_channel_tcp.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H

#include "base/threading/simple_thread.h"
#include "network/network_channel.h"

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
#include <queue>

namespace aspia {

class NetworkServerTcp;
class NetworkClientTcp;

class NetworkChannelTcp
    : public NetworkChannel,
      private SimpleThread
{
public:
    using MessageSizeType = uint32_t;

    enum class Mode { SERVER, CLIENT };

    NetworkChannelTcp(Mode mode);
    ~NetworkChannelTcp();

    void StartChannel(StatusChangeHandler handler) override;

protected:
    void Send(QByteArray&& buffer, SendCompleteHandler handler) override;
    void Send(QByteArray&& buffer) override;
    void Receive(ReceiveCompleteHandler handler) override;
    void Disconnect() override;
    bool IsDiconnecting() const override;

private:
    friend class NetworkServerTcp;
    friend class NetworkClientTcp;

    asio::io_service& io_service() { return io_service_; }
    asio::ip::tcp::socket& socket() { return socket_; }

    void DoConnect();
    void DoDisconnect();

    void DoReadMessage();
    void OnReadMessageSizeComplete(const std::error_code& code, size_t bytes_transferred);
    void OnReadMessageComplete(const std::error_code& code, size_t bytes_transferred);

    bool ReloadWriteQueue();
    void ScheduleWrite();
    void DoNextWriteTask();
    void OnWriteSizeComplete(const std::error_code& code, size_t bytes_transferred);
    void OnWriteComplete(const std::error_code& code, size_t bytes_transferred);

    // Thread implementation.
    void Run() override;

    class WriteTaskQueue
        : public std::queue<std::pair<QByteArray, SendCompleteHandler>>
    {
    public:
        void Swap(WriteTaskQueue& queue)
        {
            c.swap(queue.c); // Calls std::deque::swap.
        }
    };

    const Mode mode_;
    asio::io_service io_service_;
    std::unique_ptr<asio::io_service::work> work_;
    asio::ip::tcp::socket socket_ { io_service_ };

    StatusChangeHandler status_change_handler_;

    WriteTaskQueue work_write_queue_;
    WriteTaskQueue incoming_write_queue_;
    std::mutex incoming_write_queue_lock_;

    QByteArray write_buffer_;
    MessageSizeType write_size_ = 0;

    ReceiveCompleteHandler receive_complete_handler_;

    QByteArray read_buffer_;
    MessageSizeType read_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
