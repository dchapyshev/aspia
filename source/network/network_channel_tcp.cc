//
// PROJECT:         Aspia
// FILE:            network/network_channel_tcp.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel_tcp.h"

#include <QtEndian>

#include "base/logging.h"

namespace aspia {

namespace {

constexpr size_t kMaxMessageSize = 10 * 1024 * 1024; // 10MB

bool IsFailureCode(const std::error_code& code)
{
    return code.value() != 0;
}

} // namespace

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
        LOG(LS_ERROR) << "Failed to disable Nagle's algorithm: " << code.message();
    }

    status_change_handler_(Status::CONNECTED);
}

void NetworkChannelTcp::DoDisconnect()
{
    StopSoon();

    if (socket_.is_open())
    {
        std::error_code ignored_code;

        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_code);
        socket_.close(ignored_code);
    }

    work_.reset();

    if (!io_service_.stopped())
        io_service_.stop();
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
        LOG(LS_WARNING) << "Unable to read message size: " << code.message();
        DoDisconnect();
        return;
    }

    read_size_ = qFromBigEndian(read_size_);

    if (!read_size_ || read_size_ > kMaxMessageSize)
    {
        LOG(LS_ERROR) << "Invalid message size: " << read_size_;
        DoDisconnect();
        return;
    }

    read_buffer_.resize(read_size_);

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
        LOG(LS_WARNING) << "Unable to read message: " << code.message();
        DoDisconnect();
        return;
    }

    receive_complete_handler_(read_buffer_);
}

void NetworkChannelTcp::Receive(ReceiveCompleteHandler handler)
{
    DCHECK(handler != nullptr);
    receive_complete_handler_ = std::move(handler);
    io_service_.post(std::bind(&NetworkChannelTcp::DoReadMessage, this));
}

bool NetworkChannelTcp::ReloadWriteQueue()
{
    if (!work_write_queue_.empty())
        return false;

    {
        std::scoped_lock<std::mutex> lock(incoming_write_queue_lock_);

        if (incoming_write_queue_.empty())
            return false;

        incoming_write_queue_.Swap(work_write_queue_);
        DCHECK(incoming_write_queue_.empty());
    }

    return true;
}

void NetworkChannelTcp::ScheduleWrite()
{
    if (!work_write_queue_.empty())
        return;

    if (!ReloadWriteQueue())
        return;

    DoNextWriteTask();
}

void NetworkChannelTcp::DoNextWriteTask()
{
    write_buffer_ = std::move(work_write_queue_.front().first);
    if (write_buffer_.isEmpty())
    {
        LOG(LS_ERROR) << "Empty source buffer";
        DoDisconnect();
        return;
    }

    write_size_ = static_cast<MessageSizeType>(write_buffer_.size());

    if (write_size_ > kMaxMessageSize)
    {
        LOG(LS_ERROR) << "Invalid message size: " << write_size_;
        DoDisconnect();
        return;
    }

    write_size_ = qToBigEndian(write_size_);

    asio::async_write(socket_,
                      asio::buffer(&write_size_, sizeof(MessageSizeType)),
                      std::bind(&NetworkChannelTcp::OnWriteSizeComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannelTcp::OnWriteSizeComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(MessageSizeType))
    {
        LOG(LS_WARNING) << "Unable to write message size: " << code.message();
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

void NetworkChannelTcp::OnWriteComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != write_buffer_.size())
    {
        LOG(LS_WARNING) << "Unable to write message: " << code.message();
        DoDisconnect();
        return;
    }

    // The queue must contain the current write task.
    DCHECK(!work_write_queue_.empty());

    const SendCompleteHandler& complete_handler = work_write_queue_.front().second;

    if (complete_handler != nullptr)
        complete_handler();

    work_write_queue_.pop();

    if (work_write_queue_.empty() && !ReloadWriteQueue())
        return;

    DoNextWriteTask();
}

void NetworkChannelTcp::Send(QByteArray&& buffer, SendCompleteHandler handler)
{
    {
        std::scoped_lock<std::mutex> lock(incoming_write_queue_lock_);

        bool schedule_write = incoming_write_queue_.empty();

        incoming_write_queue_.emplace(std::move(buffer), std::move(handler));

        if (!schedule_write)
            return;
    }

    io_service_.post(std::bind(&NetworkChannelTcp::ScheduleWrite, this));
}

void NetworkChannelTcp::Send(QByteArray&& buffer)
{
    Send(std::move(buffer), nullptr);
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
