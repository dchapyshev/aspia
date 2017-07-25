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

void NetworkChannelTcp::StartChannel(StatusChangeHandler handler)
{
    DCHECK(status_change_handler_ == nullptr);
    DCHECK(handler != nullptr);

    status_change_handler_ = std::move(handler);
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
        status_change_handler_(Status::CONNECTED);
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
        status_change_handler_(Status::CONNECTED);
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoSendHello();
    }
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

    IOBuffer decrypted_buffer = encryptor_->Decrypt(read_buffer_);

    if (decrypted_buffer.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    receive_complete_handler_(std::move(decrypted_buffer));
}

void NetworkChannelTcp::Receive(ReceiveCompleteHandler handler)
{
    DCHECK(handler != nullptr);
    receive_complete_handler_ = std::move(handler);
    io_service_.post(std::bind(&NetworkChannelTcp::DoReadMessage, this));
}

void NetworkChannelTcp::ScheduleWrite()
{
    DCHECK(!write_queue_.empty());

    IOBuffer source_buffer = std::move(write_queue_.front().first);

    if (source_buffer.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    write_buffer_ = encryptor_->Encrypt(source_buffer);
    if (write_buffer_.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    write_size_ = write_buffer_.size();

    if (!write_size_ || write_size_ > kMaxMessageSize)
    {
        DoDisconnect();
        return;
    }

    write_size_ = HostByteOrderToNetwork(write_size_);

    asio::async_write(socket_,
                      asio::buffer(&write_size_, sizeof(MessageSizeType)),
                      std::bind(&NetworkChannelTcp::OnWriteSizeComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnWriteSizeComplete(const std::error_code& code,
                                            size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    asio::async_write(socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      std::bind(&NetworkChannelTcp::OnWriteComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnWriteComplete(const std::error_code& code,
                                        size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != write_buffer_.size())
    {
        DoDisconnect();
        return;
    }

    SendCompleteHandler complete_handler;

    {
        std::lock_guard<std::mutex> lock(write_queue_lock_);

        // The queue must contain the current write task.
        DCHECK(!write_queue_.empty());

        complete_handler = std::move(write_queue_.front().second);
        write_queue_.pop();

        if (!write_queue_.empty())
            ScheduleWrite();
    }

    if (complete_handler != nullptr)
        complete_handler();
}

void NetworkChannelTcp::Send(IOBuffer buffer, SendCompleteHandler handler)
{
    std::lock_guard<std::mutex> lock(write_queue_lock_);

    bool schedule_write = write_queue_.empty();

    write_queue_.push(std::make_pair<IOBuffer, SendCompleteHandler>(
        std::move(buffer), std::move(handler)));

    if (schedule_write)
        ScheduleWrite();
}

void NetworkChannelTcp::Disconnect()
{
    io_service_.post(std::bind(&NetworkChannelTcp::DoDisconnect, this));
}

bool NetworkChannelTcp::IsDiconnecting() const
{
    return IsStopping();
}

void NetworkChannelTcp::Run()
{
    work_ = std::make_unique<asio::io_service::work>(io_service_);

    std::error_code ignored_code;
    io_service_.run(ignored_code);

    if (status_change_handler_ != nullptr)
        status_change_handler_(Status::DISCONNECTED);
}

} // namespace aspia
