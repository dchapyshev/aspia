//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_tcp.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel_tcp.h"
#include "base/byte_order.h"
#include "base/logging.h"

namespace aspia {

static const size_t kMaxMessageSize = 5 * 1024 * 1024; // 5MB

static bool IsFailureCode(const std::error_code& code)
{
    return code.value() != 0;
}

NetworkChannelTcp::NetworkChannelTcp(Mode mode)
    : mode_(mode)
{
    Start();
}

NetworkChannelTcp::~NetworkChannelTcp()
{
    io_service_.dispatch(std::bind(&NetworkChannelTcp::DoDisconnect, this));
    Stop();
}

void NetworkChannelTcp::StartListening(Listener* listener)
{
    DCHECK(!listener_);
    listener_ = listener;

    io_service_.post(std::bind(&NetworkChannelTcp::DoConnect, this));
}

void NetworkChannelTcp::DoConnect()
{
    // Disable the algorithm of Nagle.
    asio::ip::tcp::no_delay option(true);

    asio::error_code code;
    socket_.set_option(option, code);

    if (IsFailureCode(code))
    {
        LOG(ERROR) << "Failed to disable Nagle's algorithm: "
                   << code.message();
    }

    const Encryptor::Mode encryptor_mode = (mode_ == Mode::CLIENT) ?
        Encryptor::Mode::CLIENT : Encryptor::Mode::SERVER;

    encryptor_ = std::make_unique<Encryptor>(encryptor_mode);

    if (mode_ == Mode::CLIENT)
    {
        DoSendHello();
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoReceiveHello();
    }
}

void NetworkChannelTcp::DoDisconnect()
{
    StopSoon();

    if (socket_.is_open())
    {
        std::error_code ignored_code;

        socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
                         ignored_code);
        socket_.close(ignored_code);
    }

    incoming_queue_.reset();

    {
        std::lock_guard<std::mutex> lock(outgoing_queue_lock_);
        outgoing_queue_.reset();
    }

    work_.reset();

    if (!io_service_.stopped())
        io_service_.stop();
}

void NetworkChannelTcp::DoSendHello()
{
    write_buffer_ = encryptor_->HelloMessage();
    if (write_buffer_.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    write_size_ = static_cast<MessageSizeType>(write_buffer_.size());
    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        DoDisconnect();
        return;
    }

    write_size_ = HostByteOrderToNetwork(write_size_);

    asio::async_write(socket_,
                      asio::buffer(&write_size_, sizeof(MessageSizeType)),
                      std::bind(&NetworkChannelTcp::OnSendHelloSizeComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnSendHelloSizeComplete(const std::error_code& code,
                                                size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    asio::async_write(socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      std::bind(&NetworkChannelTcp::OnSendHelloComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnSendHelloComplete(const std::error_code& code,
                                            size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != write_buffer_.size())
    {
        DoDisconnect();
        return;
    }

    if (mode_ == Mode::CLIENT)
    {
        DoReceiveHello();
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoStartListening();
    }
}

void NetworkChannelTcp::DoReceiveHello()
{
    asio::async_read(socket_,
                     asio::buffer(&read_size_, sizeof(MessageSizeType)),
                     std::bind(&NetworkChannelTcp::OnReceiveHelloSizeComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReceiveHelloSizeComplete(const std::error_code& code,
                                                   size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    read_size_ = NetworkByteOrderToHost(read_size_);
    if (!read_size_ || read_size_ > kMaxMessageSize)
    {
        DoDisconnect();
        return;
    }

    read_buffer_ = IOBuffer(read_size_);

    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&NetworkChannelTcp::OnReceiveHelloComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}


void NetworkChannelTcp::OnReceiveHelloComplete(const std::error_code& code,
                                               size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != read_buffer_.size())
    {
        DoDisconnect();
        return;
    }

    if (!encryptor_->ReadHelloMessage(read_buffer_))
    {
        DoDisconnect();
        return;
    }

    if (mode_ == Mode::CLIENT)
    {
        DoStartListening();
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoSendHello();
    }
}

void NetworkChannelTcp::DoStartListening()
{
    incoming_queue_ = std::make_unique<IOQueue>(
        std::bind(&NetworkChannelTcp::DoDecryptMessage, this, std::placeholders::_1));

    outgoing_queue_ = std::make_unique<IOQueue>(
        std::bind(&NetworkChannelTcp::Send, this, std::placeholders::_1));

    if (listener_)
        listener_->OnNetworkChannelConnect();

    DoReadMessage();
}

void NetworkChannelTcp::DoReadMessage()
{
    asio::async_read(socket_,
                     asio::buffer(&read_size_, sizeof(MessageSizeType)),
                     std::bind(&NetworkChannelTcp::OnReadMessageSizeComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReadMessageSizeComplete(const std::error_code& code,
                                                  size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    read_size_ = NetworkByteOrderToHost(read_size_);

    if (!read_size_ || read_size_ > kMaxMessageSize)
    {
        DoDisconnect();
        return;
    }

    read_buffer_ = IOBuffer(read_size_);

    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&NetworkChannelTcp::OnReadMessageComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReadMessageComplete(const std::error_code& code,
                                              size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != read_buffer_.size())
    {
        DoDisconnect();
        return;
    }

    if (!incoming_queue_)
        return;

    incoming_queue_->Add(std::move(read_buffer_));

    DoReadMessage();
}

void NetworkChannelTcp::DoDecryptMessage(const IOBuffer& buffer)
{
    IOBuffer message_buffer(encryptor_->Decrypt(buffer));

    if (message_buffer.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    if (listener_)
    {
        listener_->OnNetworkChannelMessage(std::move(message_buffer));
    }
}

void NetworkChannelTcp::Disconnect()
{
    if (!IsStopping())
    {
        io_service_.post(std::bind(&NetworkChannelTcp::DoDisconnect, this));
    }
}

bool NetworkChannelTcp::IsConnected() const
{
    return !IsStopping();
}

void NetworkChannelTcp::Send(const IOBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(outgoing_lock_);

    IOBuffer encrypted_buffer = encryptor_->Encrypt(buffer);

    if (!encrypted_buffer.IsEmpty() &&
        encrypted_buffer.size() <= kMaxMessageSize)
    {
        MessageSizeType message_size = static_cast<MessageSizeType>(
            encrypted_buffer.size());

        message_size = HostByteOrderToNetwork(message_size);

        std::error_code ignored_code;
        if (asio::write(socket_,
                        asio::buffer(&message_size, sizeof(MessageSizeType)),
                        ignored_code) == sizeof(MessageSizeType))
        {
            if (asio::write(socket_,
                            asio::buffer(encrypted_buffer.data(),
                                         encrypted_buffer.size()),
                            ignored_code) == encrypted_buffer.size())
            {
                return;
            }
        }
    }

    io_service_.post(std::bind(&NetworkChannelTcp::DoDisconnect, this));
}

void NetworkChannelTcp::SendAsync(IOBuffer buffer)
{
    std::lock_guard<std::mutex> lock(outgoing_queue_lock_);

    if (!outgoing_queue_)
        return;

    outgoing_queue_->Add(std::move(buffer));
}

void NetworkChannelTcp::Run()
{
    work_ = std::make_unique<asio::io_service::work>(io_service_);

    std::error_code ignored_code;
    io_service_.run(ignored_code);

    if (listener_)
    {
        listener_->OnNetworkChannelDisconnect();
        listener_ = nullptr;
    }
}

} // namespace aspia
