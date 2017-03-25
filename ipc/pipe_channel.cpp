//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_channel.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_channel.h"
#include "base/unicode.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kPipeNamePrefix[] = L"\\\\.\\pipe\\aspia.";

PipeChannel::PipeChannel(HANDLE read_pipe, HANDLE write_pipe, bool is_server) :
    read_pipe_(read_pipe),
    write_pipe_(write_pipe),
    is_server_(is_server)
{
    // Nothing
}

// static
std::wstring PipeChannel::CreatePipeName(const std::wstring& channel_id)
{
    std::wstring pipe_name(kPipeNamePrefix);
    pipe_name.append(channel_id);

    return pipe_name;
}

bool PipeChannel::Read(void* buffer, size_t buffer_size)
{
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = read_event_.native_handle();

    DWORD read_bytes = 0;
    BOOL ret;

    {
        AutoLock lock(read_lock_);

        if (!read_pipe_.IsValid())
            return false;

        ret = ReadFile(read_pipe_, buffer, buffer_size, &read_bytes, &overlapped);
    }

    if (!ret)
    {
        DWORD error = GetLastError();

        if (error == ERROR_IO_PENDING)
        {
            read_event_.Wait();

            AutoLock lock(read_lock_);

            if (!read_pipe_.IsValid())
                return false;

            DWORD transfered_bytes = 0;

            if (!GetOverlappedResult(read_pipe_, &overlapped, &transfered_bytes, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: " << GetLastError();
                return false;
            }

            return transfered_bytes == buffer_size;
        }
        else
        {
            LOG(ERROR) << "ReadFile() failed: " << error;
            return false;
        }
    }

    return read_bytes == buffer_size;
}

bool PipeChannel::Write(const void* buffer, size_t buffer_size)
{
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = write_event_.native_handle();

    DWORD written_bytes = 0;
    BOOL ret;

    {
        AutoLock lock(write_lock_);

        if (!write_pipe_.IsValid())
            return false;

        ret = WriteFile(write_pipe_, buffer, buffer_size, &written_bytes, &overlapped);
    }

    if (!ret)
    {
        DWORD error = GetLastError();

        if (error == ERROR_IO_PENDING)
        {
            write_event_.Wait();

            AutoLock lock(write_lock_);

            if (!write_pipe_.IsValid())
                return false;

            DWORD transfered_bytes = 0;

            if (!GetOverlappedResult(write_pipe_, &overlapped, &transfered_bytes, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: " << GetLastError();
                return false;
            }

            return transfered_bytes == buffer_size;
        }
        else
        {
            LOG(ERROR) << "WriteFile() failed: " << error;
            return false;
        }
    }

    return written_bytes == buffer_size;
}

bool PipeChannel::IsValid() const
{
    return read_pipe_.IsValid() && write_pipe_.IsValid();
}

bool PipeChannel::WriteMessage(const uint8_t* buffer, uint32_t size)
{
    if (Write(&size, sizeof(size)) && Write(buffer, size))
        return true;

    Stop();

    return false;
}

void PipeChannel::Disconnect()
{
    Stop();
    Wait();
}

void PipeChannel::WaitForDisconnect()
{
    Wait();
}

void PipeChannel::Worker()
{
    while (!IsTerminating())
    {
        uint32_t message_size;

        if (!Read(&message_size, sizeof(message_size)))
            break;

        if (!message_size)
            break;

        if (incomming_buffer_.Size() < message_size)
            incomming_buffer_.Resize(message_size);

        if (!Read(incomming_buffer_, message_size))
            break;

        if (!incomming_message_callback_(incomming_buffer_, message_size))
            break;
    }

    Stop();
}

void PipeChannel::OnStop()
{
    {
        AutoLock lock(read_lock_);

        if (is_server_)
            DisconnectNamedPipe(read_pipe_);

        read_pipe_.Set(nullptr);
    }

    {
        AutoLock lock(write_lock_);

        if (is_server_)
            DisconnectNamedPipe(write_pipe_);

        write_pipe_.Set(nullptr);
    }

    read_event_.Notify();
    write_event_.Notify();
}

} // namespace aspia
