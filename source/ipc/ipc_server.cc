//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "ipc/ipc_server.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/win/security_helpers.h"
#include "crypto/random.h"
#include "ipc/ipc_channel.h"

#include <asio/windows/overlapped_ptr.hpp>

namespace ipc {

namespace {

const DWORD kAcceptTimeout = 5000; // ms
const DWORD kPipeBufferSize = 512 * 1024; // 512 kB

} // namespace

Server::Server()
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext())
{
    // Nothing
}

// static
std::u16string Server::createUniqueId()
{
    static std::atomic_uint32_t last_channel_id = 0;

    uint32_t channel_id = last_channel_id++;
    uint32_t process_id = GetCurrentProcessId();
    uint32_t random_number = crypto::Random::number32();

    return base::utf16FromAscii(
        base::stringPrintf("%lu.%lu.%lu", process_id, channel_id, random_number));
}

bool Server::start(std::u16string_view channel_id, Delegate* delegate)
{
    DCHECK(delegate);

    if (channel_id.empty())
        return false;

    channel_name_ = Channel::channelName(channel_id);
    delegate_ = delegate;

    return doAccept();
}

bool Server::doAccept()
{
    std::wstring user_sid;

    if (!base::win::userSidString(&user_sid))
    {
        LOG(LS_ERROR) << "Failed to query the current user SID";
        return false;
    }

    // Create a security descriptor that gives full access to the caller and authenticated users
    // and denies access by anyone else.
    std::wstring security_descriptor =
        base::stringPrintf(L"O:%sG:%sD:(A;;GA;;;%s)(A;;GA;;;AU)",
            user_sid.c_str(),
            user_sid.c_str(),
            user_sid.c_str());

    base::win::ScopedSd sd = base::win::convertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(LS_ERROR) << "Failed to create a security descriptor";
        return false;
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    const DWORD open_mode = FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    const DWORD pipe_mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;

    stream_ = std::make_unique<asio::windows::stream_handle>(io_context_);

    std::error_code error_code;
    stream_->assign(CreateNamedPipeW(reinterpret_cast<const wchar_t*>(channel_name_.c_str()),
                                     open_mode | PIPE_ACCESS_DUPLEX,
                                     pipe_mode,
                                     1,
                                     kPipeBufferSize,
                                     kPipeBufferSize,
                                     kAcceptTimeout,
                                     &security_attributes),
                    error_code);

    if (error_code)
    {
        LOG(LS_WARNING) << "CreateNamedPipeW failed: " << error_code.message();
        return false;
    }

    asio::windows::overlapped_ptr overlapped(io_context_,
        [this](const std::error_code& error_code, size_t /* bytes_transferred */)
    {
        if (!delegate_)
            return;

        if (error_code)
        {
            delegate_->onErrorOccurred();
            return;
        }

        std::unique_ptr<Channel> channel =
            std::unique_ptr<Channel>(new Channel(io_context_, std::move(*stream_)));

        delegate_->onNewConnection(std::move(channel));

        // Waiting for the next connection.
        if (!doAccept())
            delegate_->onErrorOccurred();
    });

    if (!ConnectNamedPipe(stream_->native_handle(), overlapped.get()))
    {
        DWORD last_error = GetLastError();

        if (last_error != ERROR_IO_PENDING)
        {
            overlapped.complete(
                std::error_code(last_error, asio::error::get_system_category()), 0);
            return false;
        }
    }

    overlapped.release();
    return true;
}

} // namespace ipc
