//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/ipc/ipc_channel.h"
#include "base/strings/unicode.h"
#include "base/threading/asio_event_dispatcher.h"

#if defined(OS_WIN)
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"

#include <asio/windows/overlapped_ptr.hpp>
#include <asio/windows/stream_handle.hpp>
#endif // defined(OS_WIN)

#if defined(OS_POSIX)
#include <asio/local/stream_protocol.hpp>
#endif // defined(OS_POSIX)

#include <fmt/format.h>
#include <fmt/xchar.h>

#include <random>

namespace base {

namespace {

#if defined(OS_WIN)
const DWORD kAcceptTimeout = 5000; // ms
const DWORD kPipeBufferSize = 512 * 1024; // 512 kB
#endif

} // namespace

class IpcServer::Listener : public base::enable_shared_from_this<Listener>
{
public:
    Listener(IpcServer* server, size_t index);
    ~Listener();

    void dettach() { server_ = nullptr; }

    bool listen(asio::io_context& io_context, const QString& channel_name);

#if defined(OS_WIN)
    void onNewConnetion(const std::error_code& error_code, size_t bytes_transferred);
#elif defined(OS_POSIX)
    void onNewConnetion(const std::error_code& error_code,
                        asio::local::stream_protocol::socket socket);
#endif

private:
    IpcServer* server_;
    const size_t index_;

#if defined(OS_WIN)
    std::unique_ptr<asio::windows::stream_handle> handle_;
    std::unique_ptr<asio::windows::overlapped_ptr> overlapped_;
#elif defined(OS_POSIX)
    std::unique_ptr<asio::local::stream_protocol::acceptor> acceptor_;
    std::unique_ptr<asio::local::stream_protocol::socket> handle_;
#endif

    DISALLOW_COPY_AND_ASSIGN(Listener);
};

//--------------------------------------------------------------------------------------------------
IpcServer::Listener::Listener(IpcServer* server, size_t index)
    : server_(server),
      index_(index)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
IpcServer::Listener::~Listener() = default;

//--------------------------------------------------------------------------------------------------
bool IpcServer::Listener::listen(asio::io_context& io_context, const QString& channel_name)
{
#if defined(OS_WIN)
    std::wstring user_sid;

    if (!win::userSidString(&user_sid))
    {
        LOG(LS_ERROR) << "Failed to query the current user SID";
        return false;
    }

    // Create a security descriptor that gives full access to the caller and authenticated users
    // and denies access by anyone else.
    std::wstring security_descriptor =
        fmt::format(L"O:{0}G:{0}D:(A;;GA;;;{0})(A;;GA;;;AU)", user_sid);

    win::ScopedSd sd = win::convertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(LS_ERROR) << "Failed to create a security descriptor";
        return false;
    }

    SECURITY_ATTRIBUTES security_attributes;
    memset(&security_attributes, 0, sizeof(security_attributes));

    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    win::ScopedHandle handle(
        CreateNamedPipeW(reinterpret_cast<const wchar_t*>(channel_name.utf16()),
                         FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                         PIPE_UNLIMITED_INSTANCES,
                         kPipeBufferSize,
                         kPipeBufferSize,
                         kAcceptTimeout,
                         &security_attributes));
    if (!handle.isValid())
    {
        PLOG(LS_ERROR) << "CreateNamedPipeW failed";
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
                    std::error_code(static_cast<int>(last_error), asio::error::get_system_category()), 0);
                return false;
        }
    }

    overlapped_->complete(std::error_code(), 0);
    return true;
#else
    std::string channel_file = base::local8BitFromUtf16(channel_name);

    asio::local::stream_protocol::endpoint endpoint(channel_file);
    acceptor_ = std::make_unique<asio::local::stream_protocol::acceptor>(io_context);

    std::error_code error_code;
    acceptor_->open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->open failed: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    acceptor_->bind(endpoint, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->bind failed: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    std::string command_line = fmt::format("chmod 777 {}", channel_file.data());

    int ret = system(command_line.c_str());
    LOG(LS_INFO) << "Set security attributes: " << command_line << " (ret: " << ret << ")";

    acceptor_->listen(asio::local::stream_protocol::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "acceptor_->listen failed: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    acceptor_->async_accept(std::bind(&Listener::onNewConnetion,
                                      shared_from_this(),
                                      std::placeholders::_1,
                                      std::placeholders::_2));
    return true;
#endif
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void IpcServer::Listener::onNewConnetion(
    const std::error_code& error_code, size_t /* bytes_transferred */)
{
    if (!server_)
    {
        LOG(LS_ERROR) << "Invalid pointer";
        return;
    }

    if (error_code)
    {
        LOG(LS_ERROR) << "Error code: " << base::utf16FromLocal8Bit(error_code.message());
        server_->onErrorOccurred(FROM_HERE);
        return;
    }

    std::unique_ptr<IpcChannel> channel =
        std::unique_ptr<IpcChannel>(new IpcChannel(server_->channel_name_, std::move(*handle_), nullptr));

    server_->onNewConnection(index_, std::move(channel));
}
#endif // defined(OS_WIN)

#if defined(OS_POSIX)
//--------------------------------------------------------------------------------------------------
void IpcServer::Listener::onNewConnetion(
    const std::error_code& error_code, asio::local::stream_protocol::socket socket)
{
    if (!server_)
    {
        LOG(LS_ERROR) << "Invalid pointer";
        return;
    }

    if (error_code)
    {
        LOG(LS_ERROR) << "Error code: " << base::utf16FromLocal8Bit(error_code.message());
        server_->onErrorOccurred(FROM_HERE);
        return;
    }

    std::unique_ptr<IpcChannel> channel =
        std::unique_ptr<IpcChannel>(new IpcChannel(server_->channel_name_, std::move(socket)));

    server_->onNewConnection(index_, std::move(channel));
}
#endif // defined(OS_POSIX)

//--------------------------------------------------------------------------------------------------
IpcServer::IpcServer(QObject* parent)
    : QObject(parent),
      io_context_(AsioEventDispatcher::currentIoContext())
{
    LOG(LS_INFO) << "Ctor";

    for (size_t i = 0; i < listeners_.size(); ++i)
        listeners_[i] = base::make_local_shared<Listener>(this, i);
}

//--------------------------------------------------------------------------------------------------
IpcServer::~IpcServer()
{
    LOG(LS_INFO) << "Dtor";
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    stop();
}

//--------------------------------------------------------------------------------------------------
// static
QString IpcServer::createUniqueId()
{
    static std::atomic_uint32_t last_channel_id = 0;

    uint32_t process_id;

#if defined(OS_WIN)
    process_id = GetCurrentProcessId();
#elif defined(OS_POSIX)
    process_id = getpid();
#else
#error Not implemented
#endif

    std::random_device device;
    std::mt19937 engine(device());

    std::uniform_int_distribution<uint32_t> distance(0, std::numeric_limits<uint32_t>::max());

    uint32_t random_number = distance(engine);
    uint32_t channel_id = last_channel_id++;

    return QString("%1.%2.%3").arg(process_id).arg(channel_id).arg(random_number);
}

//--------------------------------------------------------------------------------------------------
bool IpcServer::start(const QString& channel_id, Delegate* delegate)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(delegate);

    LOG(LS_INFO) << "Starting IPC server (channel_id=" << channel_id << ")";

    if (channel_id.isEmpty())
    {
        LOG(LS_ERROR) << "Empty channel id";
        return false;
    }

    channel_name_ = IpcChannel::channelName(channel_id);
    delegate_ = delegate;

    for (size_t i = 0; i < listeners_.size(); ++i)
    {
        if (!runListener(i))
        {
            LOG(LS_ERROR) << "runListener failed (i=" << i << ")";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void IpcServer::stop()
{
    LOG(LS_INFO) << "Stopping IPC server (channel_name=" << channel_name_ << ")";
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

//--------------------------------------------------------------------------------------------------
bool IpcServer::hasPendingConnections()
{
    return !pending_.empty();
}

//--------------------------------------------------------------------------------------------------
IpcChannel* IpcServer::nextPendingConnection()
{
    if (pending_.empty())
        return nullptr;

    IpcChannel* channel = pending_.front().release();
    pending_.pop();

    return channel;
}

//--------------------------------------------------------------------------------------------------
bool IpcServer::runListener(size_t index)
{
    base::local_shared_ptr<Listener> listener = listeners_[index];
    if (!listener)
    {
        LOG(LS_ERROR) << "Unable to get listener (index=" << index << ")";
        return false;
    }

    return listener->listen(io_context_, channel_name_);
}

//--------------------------------------------------------------------------------------------------
void IpcServer::onNewConnection(size_t index, std::unique_ptr<IpcChannel> channel)
{
    LOG(LS_INFO) << "New IPC connecting (channel_name=" << channel_name_ << ")";

    pending_.emplace(std::move(channel));

    if (delegate_)
    {
        delegate_->onNewConnection();
        runListener(index);
    }
    else
    {
        LOG(LS_ERROR) << "No delegate (channel_name=" << channel_name_ << ")";
    }
}

//--------------------------------------------------------------------------------------------------
void IpcServer::onErrorOccurred(const Location& location)
{
    LOG(LS_ERROR) << "Error in IPC server (channel_name=" << channel_name_
                  << " from=" << location.toString() << ")";

    if (delegate_)
    {
        delegate_->onErrorOccurred();
    }
    else
    {
        LOG(LS_ERROR) << "No delegate (channel_name=" << channel_name_ << ")";
    }
}

} // namespace base
