//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_tcp.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_channel_tcp.h"
#include "base/logging.h"

namespace aspia {

static const std::chrono::milliseconds kWriteTimeout{ 15000 };

// static
std::unique_ptr<NetworkChannelTcp> NetworkChannelTcp::CreateClient(Socket socket)
{
    DCHECK(socket.IsValid());
    return std::unique_ptr<NetworkChannelTcp>(
        new NetworkChannelTcp(std::move(socket), Mode::CLIENT));
}

// static
std::unique_ptr<NetworkChannelTcp> NetworkChannelTcp::CreateServer(Socket socket)
{
    DCHECK(socket.IsValid());
    return std::unique_ptr<NetworkChannelTcp>(
        new NetworkChannelTcp(std::move(socket), Mode::SERVER));
}

NetworkChannelTcp::NetworkChannelTcp(Socket socket, Mode mode) :
    socket_(std::move(socket)),
    mode_(mode),
    read_event_(WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED),
    write_event_(WaitableEvent::ResetPolicy::MANUAL,
                 WaitableEvent::InitialState::NOT_SIGNALED)
{
    DWORD value = 1; // Disable the algorithm of Nagle.

    if (setsockopt(socket_,
                   IPPROTO_TCP,
                   TCP_NODELAY,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "setsockopt() failed: " << GetLastSystemErrorString();
    }
}

NetworkChannelTcp::~NetworkChannelTcp()
{
    Close();
}

bool NetworkChannelTcp::ClientKeyExchange()
{
    {
        SecureIOBuffer client_hello_message(encryptor_->HelloMessage());

        if (!WriteMessage(client_hello_message))
            return false;
    }

    size_t message_size = ReadMessageSize();
    if (!message_size)
        return false;

    {
        SecureIOBuffer server_hello_message(message_size);

        if (!ReadData(server_hello_message.data(), message_size))
            return false;

        if (!encryptor_->ReadHelloMessage(server_hello_message))
            return false;
    }

    return true;
}

bool NetworkChannelTcp::ServerKeyExchange()
{
    size_t message_size = ReadMessageSize();
    if (!message_size)
        return false;

    {
        SecureIOBuffer client_hello_message(message_size);

        if (!ReadData(client_hello_message.data(), message_size))
            return false;

        if (!encryptor_->ReadHelloMessage(client_hello_message))
            return false;
    }

    {
        SecureIOBuffer server_hello_message(encryptor_->HelloMessage());

        if (!WriteMessage(server_hello_message))
            return false;
    }

    return true;
}

bool NetworkChannelTcp::KeyExchange()
{
    if (mode_ == Mode::CLIENT)
    {
        encryptor_.reset(new Encryptor(Encryptor::Mode::CLIENT));
        if (!encryptor_)
            return false;

        if (!ClientKeyExchange())
        {
            LOG(ERROR) << "Client key exchange failure";
            return false;
        }
    }
    else
    {
        DCHECK(mode_ == Mode::SERVER);

        encryptor_.reset(new Encryptor(Encryptor::Mode::SERVER));
        if (!encryptor_)
            return false;

        if (!ServerKeyExchange())
        {
            LOG(ERROR) << "Server key exchange failure";
            return false;
        }
    }

    return true;
}

bool NetworkChannelTcp::WriteData(const uint8_t* buffer, size_t size)
{
    while (size)
    {
        write_event_.Reset();

        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = write_event_.Handle();

        WSABUF data;
        data.buf = const_cast<char*>(reinterpret_cast<const char*>(buffer));
        data.len = static_cast<ULONG>(size);

        DWORD flags = 0;
        DWORD written = 0;

        int ret = WSASend(socket_, &data, 1, nullptr, flags, &overlapped, nullptr);
        if (ret == SOCKET_ERROR)
        {
            SystemErrorCode err = GetLastSystemErrorCode();

            if (err != ERROR_IO_PENDING)
            {
                LOG(ERROR) << "WSASend() failed: " << SystemErrorCodeToString(err);
                return false;
            }
        }

        if (!write_event_.TimedWait(kWriteTimeout))
            return false;

        if (!WSAGetOverlappedResult(socket_, &overlapped, &written, FALSE, &flags))
        {
            DLOG(ERROR) << "WSAGetOverlappedResult() failed: "
                        << GetLastSystemErrorString();
            return false;
        }

        if (!written)
            return false;

        buffer += written;
        size -= written;
    }

    return true;
}

bool NetworkChannelTcp::ReadData(uint8_t* buffer, size_t size)
{
    while (size)
    {
        read_event_.Reset();

        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = read_event_.Handle();

        WSABUF data;
        data.buf = reinterpret_cast<char*>(buffer);
        data.len = static_cast<ULONG>(size);

        DWORD flags = 0;
        DWORD read = 0;

        int ret = WSARecv(socket_, &data, 1, nullptr, &flags, &overlapped, nullptr);
        if (ret == SOCKET_ERROR)
        {
            SystemErrorCode err = GetLastSystemErrorCode();

            if (err != ERROR_IO_PENDING)
            {
                LOG(ERROR) << "WSARecv() failed: " << SystemErrorCodeToString(err);
                return false;
            }
        }

        read_event_.Wait();

        if (!WSAGetOverlappedResult(socket_, &overlapped, &read, FALSE, &flags))
        {
            DLOG(ERROR) << "WSAGetOverlappedResult() failed: "
                        << GetLastSystemErrorString();
            return false;
        }

        if (!read)
            return false;

        buffer += read;
        size -= read;
    }

    return true;
}

void NetworkChannelTcp::Close()
{
    shutdown(socket_, SD_BOTH);

    read_event_.Signal();
    write_event_.Signal();
}

} // namespace aspia
