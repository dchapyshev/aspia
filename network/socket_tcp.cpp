/*
* PROJECT:         Aspia Remote Desktop
* FILE:            network/socket_tcp.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "network/socket_tcp.h"

#include "base/logging.h"

static volatile LONG _socket_ref_count = 0;

SocketTCP::SocketTCP() :
    ref_(true)
{
    // Если ни одного сокета еще не было создано.
    if (InterlockedIncrement(&_socket_ref_count) == 1)
    {
        WSADATA data;

        // Инициализируем библиотеку сокетов.
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            LOG(ERROR) << "WSAStartup() failed: " << WSAGetLastError();
            throw Exception("Unable to initialize socket library.");
        }
    }

    // Создаем сокет.
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET)
    {
        LOG(ERROR) << "socket() failed: " << WSAGetLastError();
        throw Exception("Unable to create network socket.");
    }

    //SetWriteTimeout(10000);
    //SetReadTimeout(10000);
}

SocketTCP::SocketTCP(SOCKET sock) :
    sock_(sock),
    ref_(false)
{
    //SetWriteTimeout(10000);
    //SetReadTimeout(10000);
}

SocketTCP::~SocketTCP()
{
    // Закрываем сокет.
    Close();

    if (ref_)
    {
        // Уменьшаем количество экземпляров сокета и если их больше не осталось.
        if (InterlockedDecrement(&_socket_ref_count) == 0)
        {
            // Деинициализируем библиотеку сокетов.
            WSACleanup();
        }
    }
}

void SocketTCP::Connect(const char *hostname, int port)
{
    struct sockaddr_in dest_addr;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    dest_addr.sin_addr.s_addr = inet_addr(hostname);
    if (dest_addr.sin_addr.s_addr == INADDR_NONE)
    {
        HOSTENT *host = gethostbyname(hostname);

        if (!host)
        {
            LOG(ERROR) << "gethostbyname() failed: " << WSAGetLastError();
            throw Exception("Unable to handle the host name.");
        }

        ((uint32_t *)&dest_addr.sin_addr)[0] = ((uint32_t **)host->h_addr_list)[0][0];
    }

    int ret = connect(sock_,
                      reinterpret_cast<struct sockaddr *>(&dest_addr),
                      sizeof(dest_addr));
    if (ret == SOCKET_ERROR)
    {
        LOG(ERROR) << "connect() failed: " << WSAGetLastError();
        throw Exception("Unable to establish connection.");
    }
}

int SocketTCP::Write(const char *buf, int len)
{
    return send(sock_, buf, len, 0);
}

int SocketTCP::Read(char *buf, int len)
{
    return recv(sock_, buf, len, 0);
}

void SocketTCP::Bind(const char *hostname, int port)
{
    struct sockaddr_in local_addr;

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);

    local_addr.sin_addr.s_addr =
        IsValidHostName(hostname) ? inet_addr(hostname) : htonl(INADDR_ANY);

    if (local_addr.sin_addr.s_addr == INADDR_NONE)
    {
        HOSTENT *host = gethostbyname(hostname);

        if (!host)
        {
            LOG(ERROR) << "gethostbyname() failed: " << WSAGetLastError();
            throw Exception("Unable to handle the host name.");
        }

        ((uint32_t *)&local_addr.sin_addr)[0] = ((uint32_t **)host->h_addr_list)[0][0];
    }

    if (bind(sock_,
             reinterpret_cast<struct sockaddr *>(&local_addr),
             sizeof(local_addr)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "bind() failed: " << WSAGetLastError();
        throw Exception("Unable to execute binding command.");
    }
}

void SocketTCP::Listen()
{
    if (listen(sock_, SOMAXCONN) == SOCKET_ERROR)
    {
        LOG(ERROR) << "listen() failed: " << WSAGetLastError();
        throw Exception("Unable to execute listen command.");
    }
}

std::unique_ptr<Socket> SocketTCP::Accept()
{
    struct sockaddr_in addr;
    int len = sizeof(addr);

    SOCKET client = accept(sock_, reinterpret_cast<struct sockaddr *>(&addr), &len);
    if (client == INVALID_SOCKET)
    {
        LOG(ERROR) << "accept() failed: " << WSAGetLastError();
        throw Exception("Unable to accept client connection or server stopped.");
    }

    return std::unique_ptr<Socket>(new SocketTCP(client));
}

void SocketTCP::SetWriteTimeout(int timeout)
{
    DWORD value = timeout;

    if (setsockopt(sock_,
                   SOL_SOCKET,
                   SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "setsockopt() failed: " << WSAGetLastError();
    }
}

void SocketTCP::SetReadTimeout(int timeout)
{
    DWORD value = timeout;

    if (setsockopt(sock_,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&value),
                   sizeof(value)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "setsockopt() failed: " << WSAGetLastError();
    }
}

void SocketTCP::SetNoDelay(bool enable)
{
    DWORD value = enable ? 1 : 0;

    setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&value), sizeof(value));
}

std::string SocketTCP::GetIpAddress()
{
    struct sockaddr_in addr;
    int len = sizeof(addr);

    if (getsockname(sock_, reinterpret_cast<struct sockaddr *>(&addr), &len) != SOCKET_ERROR)
    {
        return inet_ntoa(addr.sin_addr);
    }

    return "0.0.0.0";
}

void SocketTCP::Close()
{
    if (sock_ != INVALID_SOCKET)
    {
        shutdown(sock_, 2);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}
