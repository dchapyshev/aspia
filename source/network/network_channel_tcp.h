//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_tcp.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H

#include "base/threading/thread.h"
#include "crypto/encryptor.h"
#include "network/network_channel.h"
#include "protocol/io_queue.h"

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

namespace aspia {

class NetworkServerTcp;
class NetworkClientTcp;

class NetworkChannelTcp
    : public NetworkChannel,
      private Thread
{
public:
    using MessageSizeType = uint32_t;

    enum class Mode { SERVER, CLIENT };

    NetworkChannelTcp(Mode mode);
    ~NetworkChannelTcp();

    void StartListening(Listener* listener) override;

protected:
    void Disconnect() override;
    bool IsConnected() const override;
    void Send(const IOBuffer& buffer) override;
    void SendAsync(IOBuffer buffer) override;

private:
    friend class NetworkServerTcp;
    friend class NetworkClientTcp;

    asio::io_service& io_service() { return io_service_; }
    asio::ip::tcp::socket& socket() { return socket_; }

    void DoConnect();
    void DoDisconnect();

    void DoSendHelloSize();
    void OnSendHelloSize(const std::error_code& code,
                         size_t bytes_transferred);

    void DoSendHello();
    void OnSendHello(const std::error_code& code,
                     size_t bytes_transferred);

    void DoReceiveHelloSize();
    void OnReceiveHelloSize(const std::error_code& code,
                            size_t bytes_transferred);

    void DoReceiveHello();
    void OnReceiveHello(const std::error_code& code,
                        size_t bytes_transferred);

    void DoStartListening();

    void DoReadMessageSize();
    void OnReadMessageSize(const std::error_code& code,
                           size_t bytes_transferred);

    void DoReadMessage();
    void OnReadMessage(const std::error_code& code,
                       size_t bytes_transferred);

    void DoDecryptMessage(const IOBuffer& buffer);

    // Thread implementation.
    void Run() override;

    const Mode mode_;
    asio::io_service io_service_;
    std::unique_ptr<asio::io_service::work> work_;
    asio::ip::tcp::socket socket_;

    Listener* listener_ = nullptr;

    MessageSizeType message_size_ = 0;
    IOBuffer message_;

    std::unique_ptr<IOQueue> incoming_queue_;
    std::unique_ptr<IOQueue> outgoing_queue_;
    std::mutex outgoing_queue_lock_;
    std::mutex outgoing_lock_;

    std::unique_ptr<Encryptor> encryptor_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
