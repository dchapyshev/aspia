//
// PROJECT:         Aspia Remote Desktop
// FILE:            ipc/pipe_server_channel.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/pipe_server_channel.h"
#include "base/security.h"
#include "base/util.h"
#include "base/logging.h"

#include <atomic>
#include <memory>
#include <random>

namespace aspia {

static const DWORD kPipeBufferSize = 1024 * 1024;
static const uint32_t kAcceptTimeout = 10000; // 10 seconds
static std::atomic_uint32_t _last_channel_id = 0;

PipeServerChannel::PipeServerChannel(HANDLE read_pipe, HANDLE write_pipe) :
    PipeChannel(read_pipe, write_pipe, true),
    pid_(0)
{
    // Nothing
}

PipeServerChannel::~PipeServerChannel()
{
    Disconnect();
}

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

// static
PipeServerChannel* PipeServerChannel::Create(std::wstring* input_channel_id,
                                             std::wstring* output_channel_id)
{
    DCHECK(input_channel_id);
    DCHECK(output_channel_id);

    std::wstring user_sid;

    if (!GetUserSidString(&user_sid))
    {
        LOG(ERROR) << "Failed to query the current user SID";
        return nullptr;
    }

    //
    // Create a security descriptor that gives full access to the caller and
    // BUILTIN_ADMINISTRATORS and denies access by anyone else.
    // Local admins need access because the privileged host process will run
    // as a local admin which may not be the same user as the current user.
    //
    std::wstring security_descriptor =
        StringPrintfW(L"O:%sG:%sD:(A;;GA;;;%s)(A;;GA;;;BA)",
                      user_sid.c_str(),
                      user_sid.c_str(),
                      user_sid.c_str());

    ScopedSD sd = ConvertSddlToSd(security_descriptor);
    if (!sd.IsValid())
    {
        LOG(ERROR) << "Failed to create a security descriptor";
        return nullptr;
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd;
    security_attributes.bInheritHandle = FALSE;

    std::wstring server_input_channel_id = GenerateUniqueRandomChannelID();
    std::wstring server_output_channel_id = GenerateUniqueRandomChannelID();

    ScopedHandle read_pipe(CreateNamedPipeW(CreatePipeName(server_input_channel_id).c_str(),
                                            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                                            1,
                                            kPipeBufferSize,
                                            kPipeBufferSize,
                                            5000,
                                            &security_attributes));

    if (!read_pipe.IsValid())
    {
        LOG(ERROR) << "CreateNamedPipeW() failed: " << GetLastError();
        return nullptr;
    }

    ScopedHandle write_pipe(CreateNamedPipeW(CreatePipeName(server_output_channel_id).c_str(),
                                             PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                                             1,
                                             kPipeBufferSize,
                                             kPipeBufferSize,
                                             5000,
                                             &security_attributes));

    if (!write_pipe.IsValid())
    {
        LOG(ERROR) << "CreateNamedPipeW() failed: " << GetLastError();
        return nullptr;
    }

    //
    // ћы возвращаем ID каналов, к которым будет подключатьс€ клиент, а по отношению
    // к клиенту исход€щий канал сервера будет вход€щим, а вход€щий канал сервера - исход€щим.
    //
    *input_channel_id = server_output_channel_id;
    *output_channel_id = server_input_channel_id;

    return new PipeServerChannel(read_pipe.Release(), write_pipe.Release());
}

static bool ConnectToPipe(HANDLE pipe_handle)
{
    OVERLAPPED overlapped = { 0 };

    WaitableEvent event;
    overlapped.hEvent = event.native_handle();

    if (ConnectNamedPipe(pipe_handle, &overlapped))
    {
        //
        // API documentation says that this function should never return success
        // when used in overlapped mode.
        //
        LOG(ERROR) << "ConnectNamedPipe() failed: " << GetLastError();
        return false;
    }

    DWORD error = GetLastError();

    switch (error)
    {
        case ERROR_IO_PENDING:
        {
            event.Wait(kAcceptTimeout);

            DWORD dummy;

            if (!GetOverlappedResult(pipe_handle, &overlapped, &dummy, FALSE))
            {
                LOG(ERROR) << "GetOverlappedResult() failed: " << GetLastError();
                return false;
            }
        }
        break;

        case ERROR_PIPE_CONNECTED:
            break;

        default:
        {
            LOG(ERROR) << "ConnectNamedPipe() failed: " << error;
            return false;
        }
    }

    return true;
}

bool PipeServerChannel::Connect(const IncommingMessageCallback& incomming_message_callback)
{
    if (!ConnectToPipe(read_pipe_))
        return false;

    if (!ConnectToPipe(write_pipe_))
        return false;

    PipeHelloMessage message = { 0 };

    if (!Read(&message, sizeof(message)))
        return false;

    pid_ = message.pid;

    incomming_message_callback_ = incomming_message_callback;
    Start();

    return true;
}

uint32_t PipeServerChannel::GetPeerPid() const
{
    return pid_;
}

} // namespace aspia
