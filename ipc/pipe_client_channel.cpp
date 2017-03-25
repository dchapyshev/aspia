//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_client_channel.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_client_channel.h"
#include "base/logging.h"

namespace aspia {

static const uint32_t kConnectTimeout = 5000; // 5 seconds

PipeClientChannel::PipeClientChannel() :
    PipeChannel(nullptr, nullptr, false)
{
    // Nothing
}

PipeClientChannel::~PipeClientChannel()
{
    Disconnect();
}

bool PipeClientChannel::Connect(const std::wstring& input_channel_id,
                                const std::wstring& output_channel_id,
                                const IncommingMessageCallback& incomming_message_callback)
{
    write_pipe_.Set(CreateFileW(CreatePipeName(output_channel_id).c_str(),
                                GENERIC_WRITE,
                                0,
                                nullptr,
                                OPEN_EXISTING,
                                FILE_FLAG_OVERLAPPED,
                                nullptr));
    if (!write_pipe_.IsValid())
    {
        LOG(ERROR) << "CreateFileW() failed: " << GetLastError();
        return false;
    }

    read_pipe_.Set(CreateFileW(CreatePipeName(input_channel_id).c_str(),
                               GENERIC_READ,
                               0,
                               nullptr,
                               OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED,
                               nullptr));
    if (!read_pipe_.IsValid())
    {
        LOG(ERROR) << "CreateFileW() failed: " << GetLastError();
        return false;
    }

    PipeHelloMessage message = { 0 };

    message.pid = GetCurrentProcessId();

    if (!Write(&message, sizeof(message)))
        return false;

    incomming_message_callback_ = incomming_message_callback;
    Start();

    return true;
}

} // namespace aspia
