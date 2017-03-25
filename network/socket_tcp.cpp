//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/socket_tcp.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/socket_tcp.h"

#include "base/logging.h"
#include <atomic>

namespace aspia {

static std::atomic_int32_t _socket_ref_count = 0;

static const int kWriteTimeout = 15000;
static const int kMaxClientCount = 10;

SocketTCP::SocketTCP() :
    sock_(INVALID_SOCKET),
    ref_(false)
{
    // Nothing
}

SocketTCP::SocketTCP(SOCKET sock, bool ref) :
    sock_(sock),
    ref_(ref)
{
    // Nothing
}

SocketTCP::~SocketTCP()
{
    Disconnect();

    if (ref_)
    {
        // Уменьшаем количество экземпляров сокета и если их больше не осталось.
        --_socket_ref_count;

        if (!_socket_ref_count)
        {
            // Деинициализируем библиотеку сокетов.
            if (WSACleanup() == SOCKET_ERROR)
            {
                DLOG(ERROR) << "WSACleanup() failed: " << WSAGetLastError();
            }
        }
    }
}

// static
SocketTCP* SocketTCP::Create()
{
    // Если ни одного сокета еще не было создано.
    if (!_socket_ref_count)
    {
        WSADATA data = { 0 };

        // Инициализируем библиотеку сокетов.
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            LOG(ERROR) << "WSAStartup() failed: " << WSAGetLastError();
            return nullptr;
        }

        ++_socket_ref_count;
    }

    // Создаем сокет.
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        LOG(ERROR) << "socket() failed: " << WSAGetLastError();
        return nullptr;
    }

    return new SocketTCP(sock, true);
}

bool SocketTCP::Connect(const std::string& address, uint16_t port)
{
    sockaddr_in dest_addr = { 0 };

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    dest_addr.sin_addr.s_addr = inet_addr(address.c_str());
    if (dest_addr.sin_addr.s_addr == INADDR_NONE)
    {
        HOSTENT* host = gethostbyname(address.c_str());

        if (!host)
        {
            LOG(WARNING) << "gethostbyname() failed: " << WSAGetLastError();
            return false;
        }

        (reinterpret_cast<uint32_t*>(&dest_addr.sin_addr))[0] =
            (reinterpret_cast<uint32_t**>(host->h_addr_list)[0][0]);
    }

    u_long non_blocking = 1;

    if (ioctlsocket(sock_, FIONBIO, &non_blocking) == SOCKET_ERROR)
    {
        LOG(ERROR) << "ioctlsocket() failed: " << WSAGetLastError();
        return false;
    }

    // Пытаемся подключиться.
    int ret = connect(sock_,
                      reinterpret_cast<const sockaddr*>(&dest_addr),
                      sizeof(dest_addr));
    if (ret == SOCKET_ERROR)
    {
        int err = WSAGetLastError();

        // Если неблокирующий сокет не успел выполнить подключение во время вызова.
        if (err == WSAEWOULDBLOCK)
        {
            fd_set write_fds;

            FD_ZERO(&write_fds);
            FD_SET(sock_, &write_fds);

            timeval timeout = { 0 };

            timeout.tv_sec = 10; // 10 seconds.
            timeout.tv_usec = 0;

            // Ждем завершения операции.
            if (select(0, nullptr, &write_fds, nullptr, &timeout) == SOCKET_ERROR ||
                !FD_ISSET(sock_, &write_fds))
            {
                DLOG(WARNING) << "select() failed: " << WSAGetLastError();
                return false;
            }
        }
        else
        {
            LOG(WARNING) << "connect() failed: " << err;
            return false;
        }
    }

    non_blocking = 0;

    if (ioctlsocket(sock_, FIONBIO, &non_blocking) == SOCKET_ERROR)
    {
        LOG(ERROR) << "ioctlsocket() failed: " << WSAGetLastError();
        return false;
    }

    if (!EnableNagles(sock_, false))
        return false;

    if (!SetWriteTimeout(sock_, kWriteTimeout))
        return false;

    return true;
}

bool SocketTCP::Bind(uint16_t port)
{
    sockaddr_in local_addr = { 0 };

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_,
             reinterpret_cast<const sockaddr*>(&local_addr),
             sizeof(local_addr)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "bind() failed: " << WSAGetLastError();
        return false;
    }

    if (listen(sock_, kMaxClientCount) == SOCKET_ERROR)
    {
        LOG(ERROR) << "listen() failed: " << WSAGetLastError();
        return false;
    }

    return true;
}

SocketTCP* SocketTCP::Accept()
{
    sockaddr_in addr;
    int len = sizeof(addr);

    SOCKET client = accept(sock_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (client == INVALID_SOCKET)
    {
        DLOG(WARNING) << "accept() failed: " << WSAGetLastError();
        return nullptr;
    }

    if (!EnableNagles(client, false))
        return nullptr;

    if (!SetWriteTimeout(client, kWriteTimeout))
        return nullptr;

    return new SocketTCP(client, false);
}

// static
bool SocketTCP::SetWriteTimeout(SOCKET sock, int timeout)
{
    DWORD value = timeout;

    if (setsockopt(sock,
                   SOL_SOCKET,
                   SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "setsockopt() failed: " << WSAGetLastError();
        return false;
    }

    return true;
}

// static
bool SocketTCP::EnableNagles(SOCKET sock, bool enable)
{
    DWORD value = enable ? 0 : 1;

    if (setsockopt(sock,
                   IPPROTO_TCP,
                   TCP_NODELAY,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "setsockopt() failed: " << WSAGetLastError();
        return false;
    }

    return true;
}

void SocketTCP::Disconnect()
{
    AutoLock lock(shutdown_lock_);

    if (sock_ != INVALID_SOCKET)
    {
        shutdown(sock_, SD_BOTH);
        closesocket(sock_);

        sock_ = INVALID_SOCKET;
    }
}

bool SocketTCP::Writer(const char* buf, int len)
{
    while (len)
    {
        int written = send(sock_, buf, len, 0);

        if (written <= 0)
            return false;

        buf += written;
        len -= written;
    }

    return true;
}

bool SocketTCP::Write(const uint8_t* buf, uint32_t len)
{
    // Максимальный размер сообщения - 3мб.
    DCHECK(len <= 0x3FFFFF);

    uint8_t length[3];
    int count = 1;

    length[0] = len & 0x7F;

    if (len > 0x7F)
    {
        length[0] |= 0x80;
        length[count++] = len >> 7 & 0x7F;

        if (len > 0x3FFF)
        {
            length[1] |= 0x80;
            length[count++] = len >> 14 & 0xFF;
        }
    }

    // Отправляем размер данных и сами данные.
    if (!Writer(reinterpret_cast<const char*>(length), count))
        return false;

    return Writer(reinterpret_cast<const char*>(buf), len);
}

uint32_t SocketTCP::ReadSize()
{
    uint8_t byte;

    if (recv(sock_, reinterpret_cast<char*>(&byte), sizeof(byte), 0) <= 0)
        return 0;

    uint32_t size = byte & 0x7F;

    if (byte & 0x80)
    {
        if (recv(sock_, reinterpret_cast<char*>(&byte), sizeof(byte), 0) <= 0)
            return 0;

        size += (byte & 0x7F) << 7;

        if (byte & 0x80)
        {
            if (recv(sock_, reinterpret_cast<char*>(&byte), sizeof(byte), 0) <= 0)
                return 0;

            size += byte << 14;
        }
    }

    return size;
}

bool SocketTCP::Read(uint8_t* buf, uint32_t len)
{
    while (len)
    {
        int read = recv(sock_, reinterpret_cast<char*>(buf), len, 0);

        if (read <= 0)
            return false;

        buf += read;
        len -= read;
    }

    return true;
}

} // namespace aspia
