//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QTimer>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/crypto/random.h"
#include "base/ipc/ipc_channel.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"

#include <asio/windows/overlapped_ptr.hpp>
#include <asio/windows/stream_handle.hpp>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
#include <asio/local/stream_protocol.hpp>

#include <sys/stat.h>
#endif // defined(Q_OS_UNIX)

namespace {

#if defined(Q_OS_WINDOWS)
const DWORD kAcceptTimeout = 5000; // ms
const DWORD kPipeBufferSize = 512 * 1024; // 512 kB
#endif

} // namespace

class IpcServer::Listener : public std::enable_shared_from_this<Listener>
{
public:
    Listener(IpcServer* server, size_t index);
    ~Listener();

    void dettach() { server_ = nullptr; }

    bool listen(asio::io_context& io_context,
                const QString& channel_name,
                IpcServer::AccessMode access_mode,
                const QString& target_user_sid);

#if defined(Q_OS_WINDOWS)
    void onNewConnetion(const std::error_code& error_code, size_t bytes_transferred);
#elif defined(Q_OS_UNIX)
    void onNewConnetion(const std::error_code& error_code,
                        asio::local::stream_protocol::socket socket);
#endif

private:
    IpcServer* server_;
    const size_t index_;

#if defined(Q_OS_WINDOWS)
    std::unique_ptr<asio::windows::stream_handle> handle_;
    std::unique_ptr<asio::windows::overlapped_ptr> overlapped_;
#elif defined(Q_OS_UNIX)
    std::unique_ptr<asio::local::stream_protocol::acceptor> acceptor_;
    std::unique_ptr<asio::local::stream_protocol::socket> handle_;
#endif

    Q_DISABLE_COPY_MOVE(Listener)
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
bool IpcServer::Listener::listen(asio::io_context& io_context,
                                 const QString& channel_path,
                                 IpcServer::AccessMode access_mode,
                                 const QString& target_user_sid)
{
#if defined(Q_OS_WINDOWS)
    QString owner_sid;

    if (!userSidString(&owner_sid))
    {
        LOG(ERROR) << "Failed to query the current user SID";
        return false;
    }

    // Common ACE rights:
    //   GA - GENERIC_ALL for the owner (server must be able to manage pipe instances).
    //   FRFW - FILE_GENERIC_READ | FILE_GENERIC_WRITE (SYNCHRONIZE already included) for the
    //          client side. WRITE_DAC/WRITE_OWNER/DELETE intentionally NOT granted so a
    //          connected client cannot tamper with the pipe's security or kill it.
    // Mandatory label policy:
    //   NRNW - SYSTEM_MANDATORY_LABEL_NO_READ_UP | SYSTEM_MANDATORY_LABEL_NO_WRITE_UP.
    //          Blocks processes whose IL is strictly below the label from opening the pipe.
    QString security_descriptor;

    switch (access_mode)
    {
        case IpcServer::AccessMode::INTERACTIVE_USER:
        {
            // UI agent runs under an arbitrary logged-on user's token, so AU is required.
            // Medium IL label cuts off Low/Untrusted-IL processes (sandboxed browsers,
            // AppContainers). Server must additionally verify the connecting peer (session id /
            // SID) after accept.
            security_descriptor =
                QString("O:%1G:%1D:(A;;GA;;;%1)(A;;FRFW;;;AU)S:(ML;;NRNW;;;ME)").arg(owner_sid);
            break;
        }

        case IpcServer::AccessMode::SPECIFIC_USER:
        {
            if (target_user_sid.isEmpty())
            {
                LOG(ERROR) << "SPECIFIC_USER requires a target user SID";
                return false;
            }

            // Only the server itself and the specific user may connect.
            security_descriptor = QString("O:%1G:%1D:(A;;GA;;;%1)(A;;FRFW;;;%2)S:(ML;;NRNW;;;ME)")
                .arg(owner_sid, target_user_sid);
            break;
        }

        case IpcServer::AccessMode::SYSTEM_ONLY:
        {
            // Only SYSTEM may connect. Mandatory label at System IL blocks everything below.
            security_descriptor = QString("O:SYG:SYD:(A;;GA;;;SY)S:(ML;;NRNW;;;SI)");
            break;
        }

        default:
            LOG(ERROR) << "Unknown access mode:" << access_mode;
            return false;
    }

    ScopedSd sd = convertSddlToSd(security_descriptor);
    if (!sd.get())
    {
        LOG(ERROR) << "Failed to create a security descriptor";
        return false;
    }

    SECURITY_ATTRIBUTES security_attributes;
    memset(&security_attributes, 0, sizeof(security_attributes));

    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    ScopedHandle handle(
        CreateNamedPipeW(qUtf16Printable(channel_path), FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES,
            kPipeBufferSize, kPipeBufferSize, kAcceptTimeout, &security_attributes));
    if (!handle.isValid())
    {
        PLOG(ERROR) << "CreateNamedPipeW failed";
        return false;
    }

    handle_ = std::make_unique<asio::windows::stream_handle>(io_context, handle.release());

    overlapped_ = std::make_unique<asio::windows::overlapped_ptr>(
        io_context,
        std::bind(&IpcServer::Listener::onNewConnetion, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2));

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
    // TODO: full SPECIFIC_USER isolation on UNIX needs target uid (not SID). For now it shares
    // mode with INTERACTIVE_USER; peer identity is verified via path + processId checks at accept.
    Q_UNUSED(target_user_sid)

    mode_t socket_mode;
    switch (access_mode)
    {
        case AccessMode::INTERACTIVE_USER:
        case AccessMode::SPECIFIC_USER:
            socket_mode = 0666;
            break;

        case AccessMode::SYSTEM_ONLY:
            socket_mode = 0600;
            break;

        default:
            LOG(ERROR) << "Unknown access mode:" << access_mode;
            return false;
    }

    std::string channel_file = channel_path.toLocal8Bit().toStdString();

    asio::local::stream_protocol::endpoint endpoint(channel_file);
    acceptor_ = std::make_unique<asio::local::stream_protocol::acceptor>(io_context);

    std::error_code error_code;
    acceptor_->open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_->open failed:" << error_code;
        return false;
    }

    acceptor_->bind(endpoint, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_->bind failed:" << error_code;
        return false;
    }

    if (chmod(channel_file.c_str(), socket_mode) != 0)
    {
        PLOG(ERROR) << "chmod failed (path:" << channel_file << "mode:" << Qt::oct << socket_mode << ")";
        return false;
    }

    acceptor_->listen(asio::local::stream_protocol::socket::max_listen_connections, error_code);
    if (error_code)
    {
        LOG(ERROR) << "acceptor_->listen failed:" << error_code;
        return false;
    }

    acceptor_->async_accept(std::bind(
        &Listener::onNewConnetion, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    return true;
#endif
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void IpcServer::Listener::onNewConnetion(
    const std::error_code& error_code, size_t /* bytes_transferred */)
{
    if (!server_)
        return;

    if (error_code)
    {
        LOG(ERROR) << "Error code:" << error_code;
        server_->onErrorOccurred(FROM_HERE);
        return;
    }

    server_->onNewConnection(index_, new IpcChannel(server_->channel_name_, std::move(*handle_), server_));
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
void IpcServer::Listener::onNewConnetion(
    const std::error_code& error_code, asio::local::stream_protocol::socket socket)
{
    if (!server_)
        return;

    if (error_code)
    {
        LOG(ERROR) << "Error code:" << error_code;
        server_->onErrorOccurred(FROM_HERE);
        return;
    }

    server_->onNewConnection(index_, new IpcChannel(server_->channel_name_, std::move(socket), server_));
}
#endif // defined(Q_OS_UNIX)

//--------------------------------------------------------------------------------------------------
IpcServer::IpcServer(QObject* parent)
    : QObject(parent),
      io_context_(AsioEventDispatcher::ioContext())
{
    for (size_t i = 0; i < listeners_.size(); ++i)
        listeners_[i] = std::make_shared<Listener>(this, i);
}

//--------------------------------------------------------------------------------------------------
IpcServer::~IpcServer()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
// static
QString IpcServer::createUniqueId()
{
    static std::atomic_uint32_t last_channel_id = 0;

    return QString("%1.%2.%3")
        .arg(ProcessUtil::currentProcessId())
        .arg(last_channel_id++)
        .arg(QString::fromLatin1(Random::byteArray(16).toHex()));
}

//--------------------------------------------------------------------------------------------------
bool IpcServer::start(const QString& channel_name, AccessMode mode, const QString& target_user_sid)
{
    LOG(INFO) << "Starting IPC server (channel" << channel_name << "mode" << mode << ")";

    if (channel_name.isEmpty())
    {
        LOG(ERROR) << "Empty channel name";
        return false;
    }

    if (mode == AccessMode::SPECIFIC_USER && target_user_sid.isEmpty())
    {
        LOG(ERROR) << "SPECIFIC_USER mode requires a non-empty target user SID";
        return false;
    }

    channel_name_ = channel_name;
    access_mode_ = mode;
    target_user_sid_ = target_user_sid;

    for (size_t i = 0; i < listeners_.size(); ++i)
    {
        if (!runListener(i))
        {
            LOG(ERROR) << "runListener failed (i:" << i << ")";
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void IpcServer::stop()
{
    LOG(INFO) << "Stopping IPC server (channel" << channel_name_ << ")";

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
    return !pending_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
IpcChannel* IpcServer::nextPendingConnection()
{
    if (pending_.isEmpty())
        return nullptr;

    IpcChannel* channel = pending_.front();
    channel->setParent(nullptr);

    pending_.pop_front();
    return channel;
}

//--------------------------------------------------------------------------------------------------
bool IpcServer::runListener(size_t index)
{
    std::shared_ptr<Listener> listener = listeners_[index];
    if (!listener)
    {
        LOG(ERROR) << "Unable to get listener (index" << index << ")";
        return false;
    }

    return listener->listen(
        io_context_, IpcChannel::channelPath(channel_name_), access_mode_, target_user_sid_);
}

//--------------------------------------------------------------------------------------------------
void IpcServer::onNewConnection(size_t index, IpcChannel* channel)
{
    LOG(INFO) << "New IPC connecting (channel" << channel_name_ << ")";

    if (!runListener(index))
    {
        LOG(ERROR) << "Unable to restart listener" << index;
        emit sig_errorOccurred();
        return;
    }

    pending_.emplace_back(channel);
    emit sig_newConnection();
}

//--------------------------------------------------------------------------------------------------
void IpcServer::onErrorOccurred(const Location& location)
{
    LOG(ERROR) << "Error in IPC server (channel" << channel_name_ << "from" << location << ")";
    emit sig_errorOccurred();
}
