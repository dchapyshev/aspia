//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/ipc/ipc_server.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/ipc/ipc_channel.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"

#include <asio/post.hpp>
#include <asio/windows/overlapped_ptr.hpp>
#include <asio/windows/stream_handle.hpp>

namespace base {

namespace {

const DWORD kAcceptTimeout = 5000; // ms
const DWORD kPipeBufferSize = 512 * 1024; // 512 kB

} // namespace

class IpcServer::Listener : public std::enable_shared_from_this<Listener>
{
public:
    Listener(IpcServer* server, size_t index);
    ~Listener();

    void dettach() { server_ = nullptr; }

    bool listen(asio::io_context& io_context, std::u16string_view channel_name);
    void onNewConnetion(const std::error_code& error_code, size_t bytes_transferred);

private:
    IpcServer* server_;
    const size_t index_;

    std::unique_ptr<asio::windows::stream_handle> handle_;
    std::unique_ptr<asio::windows::overlapped_ptr> overlapped_;

    DISALLOW_COPY_AND_ASSIGN(Listener);
};

IpcServer::Listener::Listener(IpcServer* server, size_t index)
    : server_(server),
      index_(index)
{
    // Nothing
}

IpcServer::Listener::~Listener() = default;

bool IpcServer::Listener::listen(asio::io_context& io_context, std::u16string_view channel_name)
{
    std::wstring user_sid;

    if (!win::userSidString(&user_sid))
    {
        LOG(LS_ERROR) << "Failed to query the current user SID";
        return false;
    }

    // Create a security descriptor that gives full access to the caller and authenticated users
    // and denies access by anyone else.
    std::wstring security_descriptor =
        stringPrintf(L"O:%sG:%sD:(A;;GA;;;%s)(A;;GA;;;AU)",
                     user_sid.c_str(),
                     user_sid.c_str(),
                     user_sid.c_str());

    win::ScopedSd sd = win::convertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(LS_ERROR) << "Failed to create a security descriptor";
        return false;
    }

    SECURITY_ATTRIBUTES security_attributes = { 0 };
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    win::ScopedHandle handle(
        CreateNamedPipeW(reinterpret_cast<const wchar_t*>(channel_name.data()),
                         FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                         PIPE_UNLIMITED_INSTANCES,
                         kPipeBufferSize,
                         kPipeBufferSize,
                         kAcceptTimeout,
                         &security_attributes));
    if (!handle.isValid())
    {
        PLOG(LS_WARNING) << "CreateNamedPipeW failed";
        return false;
    }

    handle_ = std::make_unique<asio::windows::stream_handle>(io_context, handle.release());

    overlapped_ = std::make_unique<asio::windows::overlapped_ptr>(
        io_context,
        std::bind(&IpcServer::Listener::onNewConnetion,
                  shared_from_this(),
                  std::placeholders::_1,
                  std::placeholders::_2));

    if (!ConnectNamedPipe(handle_->native_handle(), overlapped_->get()))
    {
        DWORD last_error = GetLastError();

        switch (last_error)
        {
            case ERROR_PIPE_CONNECTED:
                break;

            case ERROR_IO_PENDING:
                overlapped_->release();
                return true;

            default:
                overlapped_->complete(
                    std::error_code(last_error, asio::error::get_system_category()), 0);
                return false;
        }
    }

    overlapped_->complete(std::error_code(), 0);
    return true;
}

void IpcServer::Listener::onNewConnetion(
    const std::error_code& error_code, size_t /* bytes_transferred */)
{
    if (!server_)
        return;

    if (error_code)
    {
        server_->onErrorOccurred(FROM_HERE);
        return;
    }

    std::unique_ptr<IpcChannel> channel =
        std::unique_ptr<IpcChannel>(new IpcChannel(server_->channel_name_, std::move(*handle_)));

    server_->onNewConnection(index_, std::move(channel));
}

IpcServer::IpcServer()
    : io_context_(MessageLoop::current()->pumpAsio()->ioContext())
{
    for (size_t i = 0; i < listeners_.size(); ++i)
        listeners_[i] = std::make_shared<Listener>(this, i);
}

IpcServer::~IpcServer()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    stop();
}

// static
std::u16string IpcServer::createUniqueId()
{
    static std::atomic_uint32_t last_channel_id = 0;

    uint32_t channel_id = last_channel_id++;
    uint32_t process_id = GetCurrentProcessId();
    uint32_t random_number = Random::number32();

    return utf16FromAscii(
        stringPrintf("%lu.%lu.%lu", process_id, channel_id, random_number));
}

bool IpcServer::start(std::u16string_view channel_id, Delegate* delegate)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(delegate);

    if (channel_id.empty())
    {
        DLOG(LS_ERROR) << "Empty channel id";
        return false;
    }

    channel_name_ = IpcChannel::channelName(channel_id);
    delegate_ = delegate;

    for (size_t i = 0; i < listeners_.size(); ++i)
    {
        if (!runListener(i))
            return false;
    }

    return true;
}

void IpcServer::stop()
{
    delegate_ = nullptr;

    for (size_t i = 0; i < listeners_.size(); ++i)
    {
        if (listeners_[i])
        {
            listeners_[i]->dettach();
            listeners_[i].reset();
        }
    }
}

bool IpcServer::runListener(size_t index)
{
    std::shared_ptr<Listener> listener = listeners_[index];
    if (!listener)
        return false;

    return listener->listen(io_context_, channel_name_);
}

void IpcServer::onNewConnection(size_t index, std::unique_ptr<IpcChannel> channel)
{
    if (delegate_)
    {
        delegate_->onNewConnection(std::move(channel));
        runListener(index);
    }
}

void IpcServer::onErrorOccurred(const Location& location)
{
    LOG(LS_WARNING) << "Error in IPC server with name: " << channel_name_
                    << " (" << location.toString() << ")";

    if (delegate_)
        delegate_->onErrorOccurred();
}

} // namespace base
