//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_channel.h"
#include "base/version_helpers.h"
#include "base/security_helpers.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

#include <random>

namespace aspia {

static const WCHAR kPipeNamePrefix[] = L"\\\\.\\pipe\\aspia.";
static const DWORD kPipeBufferSize = 512 * 1024; // 512 KB
static const std::chrono::milliseconds kConnectTimeout{ 5000 };

static std::atomic_uint32_t _last_channel_id = 0;

static std::wstring GenerateUniqueRandomChannelID()
{
    uint32_t process_id = GetCurrentProcessId();
    uint32_t last_channel_id = _last_channel_id;

    std::random_device device;
    std::uniform_int_distribution<uint32_t> uniform(0, std::numeric_limits<uint32_t>::max());

    uint32_t random = uniform(device);

    ++_last_channel_id;

    return StringPrintfW(L"%u.%u.%u", process_id, last_channel_id, random);
}

static std::wstring CreatePipeName(const std::wstring& channel_id)
{
    std::wstring pipe_name(kPipeNamePrefix);
    pipe_name.append(channel_id);

    return pipe_name;
}

// static
std::unique_ptr<PipeChannel> PipeChannel::CreateServer(std::wstring& input_channel_id,
                                                       std::wstring& output_channel_id)
{
    std::wstring user_sid;

    if (!GetUserSidString(user_sid))
    {
        LOG(ERROR) << "Failed to query the current user SID";
        return nullptr;
    }

    // Create a security descriptor that gives full access to the caller and
    // BUILTIN_ADMINISTRATORS and denies access by anyone else.
    // Local admins need access because the privileged host process will run
    // as a local admin which may not be the same user as the current user.
    std::wstring security_descriptor =
        StringPrintfW(L"O:%sG:%sD:(A;;GA;;;%s)(A;;GA;;;BA)",
                      user_sid.c_str(),
                      user_sid.c_str(),
                      user_sid.c_str());

    ScopedSd sd = ConvertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(ERROR) << "Failed to create a security descriptor";
        return nullptr;
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    std::wstring server_input_channel_id = GenerateUniqueRandomChannelID();
    std::wstring server_output_channel_id = GenerateUniqueRandomChannelID();

    DWORD open_mode = FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    DWORD pipe_mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;

    // Windows XP/2003 does't support this flag.
    if (IsWindowsVistaOrGreater())
        pipe_mode |= PIPE_REJECT_REMOTE_CLIENTS;

    ScopedHandle read_pipe(CreateNamedPipeW(CreatePipeName(server_input_channel_id).c_str(),
                                            open_mode | PIPE_ACCESS_INBOUND,
                                            pipe_mode,
                                            1,
                                            kPipeBufferSize,
                                            kPipeBufferSize,
                                            5000,
                                            &security_attributes));

    if (!read_pipe.IsValid())
    {
        LOG(ERROR) << "CreateNamedPipeW() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    ScopedHandle write_pipe(CreateNamedPipeW(CreatePipeName(server_output_channel_id).c_str(),
                                             open_mode | PIPE_ACCESS_OUTBOUND,
                                             pipe_mode,
                                             1,
                                             kPipeBufferSize,
                                             kPipeBufferSize,
                                             5000,
                                             &security_attributes));

    if (!write_pipe.IsValid())
    {
        LOG(ERROR) << "CreateNamedPipeW() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    // We return the ID of the channels to which the client will be connected,
    // and in relation to the client, the outgoing server channel will be incoming,
    // and the incoming server channel will be outgoing.
    input_channel_id = std::move(server_output_channel_id);
    output_channel_id = std::move(server_input_channel_id);

    return std::unique_ptr<PipeChannel>(new PipeChannel(read_pipe.Release(),
                                                        write_pipe.Release(),
                                                        Mode::SERVER));
}

// static
std::unique_ptr<PipeChannel> PipeChannel::CreateClient(const std::wstring& input_channel_id,
                                                       const std::wstring& output_channel_id)
{
    DWORD flags = SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                  FILE_FLAG_OVERLAPPED;

    ScopedHandle write_pipe(CreateFileW(CreatePipeName(output_channel_id).c_str(),
                                        GENERIC_WRITE,
                                        0,
                                        nullptr,
                                        OPEN_EXISTING,
                                        flags,
                                        nullptr));
    if (!write_pipe.IsValid())
    {
        LOG(ERROR) << "CreateFileW() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    ScopedHandle read_pipe(CreateFileW(CreatePipeName(input_channel_id).c_str(),
                                       GENERIC_READ,
                                       0,
                                       nullptr,
                                       OPEN_EXISTING,
                                       flags,
                                       nullptr));
    if (!read_pipe.IsValid())
    {
        LOG(ERROR) << "CreateFileW() failed: " << GetLastSystemErrorString();
        return nullptr;
    }

    return std::unique_ptr<PipeChannel>(new PipeChannel(read_pipe.Release(),
                                                        write_pipe.Release(),
                                                        Mode::CLIENT));
}

PipeChannel::PipeChannel(HANDLE read_pipe, HANDLE write_pipe, Mode mode)
    : read_pipe_(read_pipe),
      write_pipe_(write_pipe),
      mode_(mode)
{
    // Nothing
}

PipeChannel::~PipeChannel()
{
    Close();
    Stop();
}

bool PipeChannel::Read(void* buffer, size_t buffer_size)
{
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = read_event_.Handle();

    DWORD read_bytes = 0;
    BOOL ret;

    {
        std::lock_guard<std::mutex> lock(read_lock_);

        if (!read_pipe_.IsValid())
            return false;

        ret = ReadFile(read_pipe_, buffer, static_cast<DWORD>(buffer_size),
                       &read_bytes, &overlapped);
    }

    if (!ret)
    {
        DWORD error = GetLastError();

        if (error == ERROR_IO_PENDING)
        {
            read_event_.Wait();

            std::lock_guard<std::mutex> lock(read_lock_);

            if (!read_pipe_.IsValid())
                return false;

            DWORD transfered_bytes = 0;

            if (!GetOverlappedResult(read_pipe_, &overlapped, &transfered_bytes, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: "
                           << GetLastSystemErrorString();
                return false;
            }

            return transfered_bytes == buffer_size;
        }

        LOG(ERROR) << "ReadFile() failed: " << SystemErrorCodeToString(error);
        return false;
    }

    return read_bytes == buffer_size;
}

bool PipeChannel::Write(const void* buffer, size_t buffer_size)
{
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = write_event_.Handle();

    DWORD written_bytes = 0;
    BOOL ret;

    {
        std::lock_guard<std::mutex> lock(write_lock_);

        if (!write_pipe_.IsValid())
            return false;

        ret = WriteFile(write_pipe_, buffer, static_cast<DWORD>(buffer_size),
                        &written_bytes, &overlapped);
    }

    if (!ret)
    {
        DWORD error = GetLastError();

        if (error == ERROR_IO_PENDING)
        {
            write_event_.Wait();

            std::lock_guard<std::mutex> lock(write_lock_);

            if (!write_pipe_.IsValid())
                return false;

            DWORD transfered_bytes = 0;

            if (!GetOverlappedResult(write_pipe_, &overlapped, &transfered_bytes, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: "
                           << GetLastSystemErrorString();
                return false;
            }

            return transfered_bytes == buffer_size;
        }

        LOG(ERROR) << "WriteFile() failed: " << SystemErrorCodeToString(error);
        return false;
    }

    return written_bytes == buffer_size;
}

void PipeChannel::Send(const IOBuffer& buffer)
{
    if (!buffer.IsEmpty())
    {
        uint32_t message_size = static_cast<uint32_t>(buffer.size());

        if (Write(&message_size, sizeof(message_size)))
        {
            if (Write(buffer.data(), buffer.size()))
                return;
        }
    }

    Close();
}

static bool ConnectToClientPipe(HANDLE pipe_handle)
{
    OVERLAPPED overlapped = { 0 };

    WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                        WaitableEvent::InitialState::NOT_SIGNALED);

    overlapped.hEvent = event.Handle();

    if (ConnectNamedPipe(pipe_handle, &overlapped))
    {
        // API documentation says that this function should never return success
        // when used in overlapped mode.
        LOG(ERROR) << "ConnectNamedPipe() failed: " << GetLastSystemErrorString();
        return false;
    }

    DWORD error = GetLastError();

    switch (error)
    {
        case ERROR_IO_PENDING:
        {
            event.TimedWait(kConnectTimeout);

            DWORD dummy;

            if (!GetOverlappedResult(pipe_handle, &overlapped, &dummy, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: "
                           << GetLastSystemErrorString();
                return false;
            }
        }
        break;

        case ERROR_PIPE_CONNECTED:
            break;

        default:
        {
            LOG(ERROR) << "ConnectNamedPipe() failed: "
                       << SystemErrorCodeToString(error);
            return false;
        }
    }

    return true;
}

bool PipeChannel::Connect(uint32_t user_data, Delegate* delegate)
{
    DCHECK(delegate);

    if (mode_ == Mode::SERVER)
    {
        if (!ConnectToClientPipe(read_pipe_) || !ConnectToClientPipe(write_pipe_))
            return false;

        if (!Read(&user_data_, sizeof(user_data_)))
            return false;

        if (!Write(&user_data, sizeof(user_data)))
            return false;
    }
    else
    {
        DCHECK(mode_ == Mode::CLIENT);

        if (!Write(&user_data, sizeof(user_data)))
            return false;

        if (!Read(&user_data_, sizeof(user_data_)))
            return false;
    }

    delegate_ = delegate;
    Start();

    return true;
}

void PipeChannel::Close()
{
    {
        std::lock_guard<std::mutex> lock(read_lock_);

        if (mode_ == Mode::SERVER)
            DisconnectNamedPipe(read_pipe_);

        read_pipe_.Reset();
    }

    {
        std::lock_guard<std::mutex> lock(write_lock_);

        if (mode_ == Mode::SERVER)
            DisconnectNamedPipe(write_pipe_);

        write_pipe_.Reset();
    }

    read_event_.Signal();
    write_event_.Signal();
}

void PipeChannel::Wait()
{
    Join();
}

void PipeChannel::Run()
{
    delegate_->OnPipeChannelConnect(user_data_);

    while (!IsStopping())
    {
        uint32_t message_size = 0;

        if (Read(&message_size, sizeof(message_size)))
        {
            if (message_size)
            {
                IOBuffer buffer(message_size);

                if (Read(buffer.data(), buffer.size()))
                {
                    delegate_->OnPipeChannelMessage(buffer);
                    continue;
                }
            }
        }

        break;
    }

    Close();
    delegate_->OnPipeChannelDisconnect();
}

} // namespace aspia
