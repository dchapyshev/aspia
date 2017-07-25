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
    : socket_(io_service_),
      mode_(mode)
{
    static_assert(sizeof(message_size_) == sizeof(MessageSizeType),
                  "Wrong message size field");
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
        DoSendHelloSize();
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoReceiveHelloSize();
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

void NetworkChannelTcp::DoSendHelloSize()
{
    message_ = encryptor_->HelloMessage();
    if (message_.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    message_size_ = static_cast<MessageSizeType>(message_.size());

    asio::async_write(socket_,
                      asio::buffer(&message_size_, sizeof(MessageSizeType)),
                      std::bind(&NetworkChannelTcp::OnSendHelloSize,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnSendHelloSize(const std::error_code& code,
                                        size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    DoSendHello();
}

void NetworkChannelTcp::DoSendHello()
{
    asio::async_write(socket_,
                      asio::buffer(message_.data(), message_.size()),
                      std::bind(&NetworkChannelTcp::OnSendHello,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnSendHello(const std::error_code& code,
                                    size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != message_.size())
    {
        DoDisconnect();
        return;
    }

    if (mode_ == Mode::CLIENT)
    {
        DoReceiveHelloSize();
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);
        DoStartListening();
    }
}

void NetworkChannelTcp::DoReceiveHelloSize()
{
    asio::async_read(socket_,
                     asio::buffer(&message_size_, sizeof(MessageSizeType)),
                     std::bind(&NetworkChannelTcp::OnReceiveHelloSize,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReceiveHelloSize(const std::error_code& code,
                                           size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    DoReceiveHello();
}

void NetworkChannelTcp::DoReceiveHello()
{
    message_ = IOBuffer(message_size_);

    asio::async_read(socket_,
                     asio::buffer(message_.data(), message_.size()),
                     std::bind(&NetworkChannelTcp::OnReceiveHello,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReceiveHello(const std::error_code& code,
                                       size_t bytes_transferred)
{
    if (IsFailureCode(code) || message_size_ != bytes_transferred)
    {
        DoDisconnect();
        return;
    }

    if (!encryptor_->ReadHelloMessage(message_))
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
        DoSendHelloSize();
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

    DoReadMessageSize();
}

void NetworkChannelTcp::DoReadMessageSize()
{
    asio::async_read(socket_,
                     asio::buffer(&message_size_, sizeof(MessageSizeType)),
                     std::bind(&NetworkChannelTcp::OnReadMessageSize,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReadMessageSize(const std::error_code& code,
                                          size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        DoDisconnect();
        return;
    }

    message_size_ = NetworkByteOrderToHost(message_size_);

    if (message_size_ > kMaxMessageSize)
    {
        DoDisconnect();
        return;
    }

    DoReadMessage();
}

void NetworkChannelTcp::DoReadMessage()
{
    message_ = IOBuffer(message_size_);

    asio::async_read(socket_,
                     asio::buffer(message_.data(), message_.size()),
                     std::bind(&NetworkChannelTcp::OnReadMessage,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannelTcp::OnReadMessage(const std::error_code& code,
                                      size_t bytes_transferred)
{
    if (IsFailureCode(code) || message_size_ != bytes_transferred)
    {
        DoDisconnect();
        return;
    }

    if (!incoming_queue_)
        return;

    incoming_queue_->Add(std::move(message_));

    DoReadMessageSize();
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
