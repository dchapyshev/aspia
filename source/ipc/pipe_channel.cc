//
// PROJECT:         Aspia
// FILE:            ipc/pipe_channel.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_channel.h"

#include "base/security_helpers.h"
#include "base/scoped_object.h"
#include "base/strings/string_printf.h"
#include "base/logging.h"
#include "crypto/random.h"
#include "ipc/pipe_channel_proxy.h"

namespace aspia {

namespace {

constexpr wchar_t kPipeNamePrefix[] = L"\\\\.\\pipe\\aspia.";
constexpr DWORD kPipeBufferSize = 512 * 1024; // 512 kB

bool IsFailureCode(const std::error_code& code)
{
    return code.value() != 0;
}

std::wstring GenerateUniqueRandomChannelID()
{
    static std::atomic_uint32_t last_channel_id = 0;
    uint32_t channel_id = last_channel_id++;

    return StringPrintf(L"%lu.%lu.%lu",
                        GetCurrentProcessId(),
                        channel_id,
                        CreateRandomNumber());
}

std::wstring CreatePipeName(const std::wstring& channel_id)
{
    std::wstring pipe_name(kPipeNamePrefix);
    pipe_name.append(channel_id);

    return pipe_name;
}

} // namespace

// static
std::unique_ptr<PipeChannel> PipeChannel::CreateServer(std::wstring& channel_id)
{
    std::wstring user_sid;

    if (!GetUserSidString(user_sid))
    {
        LOG(LS_ERROR) << "Failed to query the current user SID";
        return nullptr;
    }

    // Create a security descriptor that gives full access to the caller and authenticated users
    // and denies access by anyone else.
    std::wstring security_descriptor =
        StringPrintf(L"O:%sG:%sD:(A;;GA;;;%s)(A;;GA;;;AU)",
                     user_sid.c_str(),
                     user_sid.c_str(),
                     user_sid.c_str());

    ScopedSd sd = ConvertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(LS_ERROR) << "Failed to create a security descriptor";
        return nullptr;
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    channel_id = GenerateUniqueRandomChannelID();

    DWORD open_mode = FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    DWORD pipe_mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;

    ScopedHandle pipe(CreateNamedPipeW(CreatePipeName(channel_id).c_str(),
                                       open_mode | PIPE_ACCESS_DUPLEX,
                                       pipe_mode,
                                       1,
                                       kPipeBufferSize,
                                       kPipeBufferSize,
                                       5000,
                                       &security_attributes));

    if (!pipe.IsValid())
    {
        PLOG(LS_ERROR) << "CreateNamedPipeW failed";
        return nullptr;
    }

    return std::unique_ptr<PipeChannel>(new PipeChannel(pipe.Release(), Mode::SERVER));
}

// static
std::unique_ptr<PipeChannel> PipeChannel::CreateClient(const std::wstring& channel_id)
{
    DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED;

    ScopedHandle pipe(CreateFileW(CreatePipeName(channel_id).c_str(),
                                  GENERIC_WRITE | GENERIC_READ,
                                  0,
                                  nullptr,
                                  OPEN_EXISTING,
                                  flags,
                                  nullptr));
    if (!pipe.IsValid())
    {
        PLOG(LS_ERROR) << "CreateFileW failed";
        return nullptr;
    }

    return std::unique_ptr<PipeChannel>(new PipeChannel(pipe.Release(), Mode::CLIENT));
}

PipeChannel::PipeChannel(HANDLE pipe, Mode mode)
    : mode_(mode)
{
    proxy_.reset(new PipeChannelProxy(this));

    std::error_code ignored_code;
    stream_.assign(pipe, ignored_code);

    Start();
}

PipeChannel::~PipeChannel()
{
    proxy_->WillDestroyCurrentChannel();
    proxy_ = nullptr;

    io_service_.dispatch(std::bind(&PipeChannel::DoDisconnect, this));
    Stop();
}

bool PipeChannel::ReloadWriteQueue()
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

void PipeChannel::ScheduleWrite()
{
    if (!work_write_queue_.empty())
        return;

    if (!ReloadWriteQueue())
        return;

    DoNextWriteTask();
}

void PipeChannel::DoNextWriteTask()
{
    write_buffer_ = std::move(work_write_queue_.front().first);

    if (write_buffer_.IsEmpty())
    {
        DoDisconnect();
        return;
    }

    write_size_ = static_cast<uint32_t>(write_buffer_.Size());

    asio::async_write(stream_,
                      asio::buffer(&write_size_, sizeof(uint32_t)),
                      std::bind(&PipeChannel::OnWriteSizeComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void PipeChannel::OnWriteSizeComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(uint32_t))
    {
        DoDisconnect();
        return;
    }

    asio::async_write(stream_,
                      asio::buffer(write_buffer_.Data(), write_buffer_.Size()),
                      std::bind(&PipeChannel::OnWriteComplete,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void PipeChannel::OnWriteComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != write_buffer_.Size())
    {
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

void PipeChannel::Send(IOBuffer&& buffer, SendCompleteHandler handler)
{
    {
        std::scoped_lock<std::mutex> lock(incoming_write_queue_lock_);

        bool schedule_write = incoming_write_queue_.empty();

        incoming_write_queue_.emplace(std::move(buffer), std::move(handler));

        if (!schedule_write)
            return;
    }

    io_service_.post(std::bind(&PipeChannel::ScheduleWrite, this));
}

void PipeChannel::Send(IOBuffer&& buffer)
{
    Send(std::move(buffer), nullptr);
}

bool PipeChannel::Connect(uint32_t& user_data, DisconnectHandler disconnect_handler)
{
    disconnect_handler_ = std::move(disconnect_handler);

    std::error_code ignored_code;
    uint32_t remote_user_data;

    if (mode_ == Mode::SERVER)
    {
        if (!ConnectNamedPipe(stream_.native_handle(), nullptr))
        {
            PLOG(LS_ERROR) << "ConnectNamedPipe failed";
            return false;
        }

        if (asio::read(stream_,
                       asio::buffer(&remote_user_data, sizeof(remote_user_data)),
                       ignored_code) != sizeof(remote_user_data))
            return false;

        if (asio::write(stream_,
                        asio::buffer(&user_data, sizeof(user_data)),
                        ignored_code) != sizeof(user_data))
            return false;
    }
    else
    {
        DCHECK(mode_ == Mode::CLIENT);

        if (asio::write(stream_,
                        asio::buffer(&user_data, sizeof(user_data)),
                        ignored_code) != sizeof(user_data))
            return false;

        if (asio::read(stream_,
                       asio::buffer(&remote_user_data, sizeof(remote_user_data)),
                       ignored_code) != sizeof(remote_user_data))
            return false;
    }

    user_data = remote_user_data;
    return true;
}

void PipeChannel::ScheduleRead()
{
    asio::async_read(stream_,
                     asio::buffer(&read_size_, sizeof(uint32_t)),
                     std::bind(&PipeChannel::OnReadSizeComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void PipeChannel::OnReadSizeComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != sizeof(uint32_t))
    {
        DoDisconnect();
        return;
    }

    if (!read_size_)
    {
        DoDisconnect();
        return;
    }

    read_buffer_ = IOBuffer::Create(read_size_);

    asio::async_read(stream_,
                     asio::buffer(read_buffer_.Data(), read_buffer_.Size()),
                     std::bind(&PipeChannel::OnReadComplete,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void PipeChannel::OnReadComplete(const std::error_code& code, size_t bytes_transferred)
{
    if (IsFailureCode(code) || bytes_transferred != read_buffer_.Size())
    {
        DoDisconnect();
        return;
    }

    receive_complete_handler_(read_buffer_);
}

void PipeChannel::Receive(ReceiveCompleteHandler handler)
{
    DCHECK(handler != nullptr);
    receive_complete_handler_ = std::move(handler);
    io_service_.post(std::bind(&PipeChannel::ScheduleRead, this));
}

void PipeChannel::DoDisconnect()
{
    StopSoon();

    std::error_code ignored_code;
    stream_.close(ignored_code);

    work_.reset();

    if (!io_service_.stopped())
        io_service_.stop();
}

void PipeChannel::Disconnect()
{
    io_service_.post(std::bind(&PipeChannel::DoDisconnect, this));
}

bool PipeChannel::IsDisconnecting() const
{
    return IsStopping();
}

void PipeChannel::Run()
{
    work_ = std::make_unique<asio::io_service::work>(io_service_);

    std::error_code ignored_code;
    io_service_.run(ignored_code);

    if (disconnect_handler_ != nullptr)
        disconnect_handler_();
}

} // namespace aspia
