/*
* PROJECT:         Aspia Remote Desktop
* FILE:            network/socket.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "network/socket.h"

#include "base/logging.h"

static const size_t kMaxHostNameLength = 64;

Socket::Socket() :
    write_buffer_size_(0),
    read_buffer_size_(0)
{
    crypto_.reset(new EncryptorAES("my_password"));
}

Socket::~Socket()
{
    // Nothing
}

// static
bool Socket::IsHostnameChar(char c)
{
    // В имени хоста разрешены символы: 0-9a-zA-Z. _-

    if (isalnum(c) != 0)
        return true;

    if (c == '.' || c == ' ' || c == '_' || c == '-')
        return true;

    return false;
}

// static
bool Socket::IsValidHostName(const char *hostname)
{
    size_t length = strlen(hostname);

    if (!length || length > kMaxHostNameLength)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (!IsHostnameChar(hostname[i]))
            return false;
    }

    return true;
}

// static
bool Socket::IsValidPort(int port)
{
    if (port == 0 || port >= 65535)
        return false;

    return true;
}

void Socket::Reader(uint8_t *buf, int len)
{
    int total_read = 0;
    int left = len;

    while (total_read < len)
    {
        int read = Read(buf + total_read, left);

        if (read <= 0)
        {
            Disconnect();
            throw Exception("Unable to read data from network.");
        }

        left -= read;
        total_read += read;
    }
}

void Socket::Writer(const uint8_t *buf, int len)
{
    int total_written = 0;
    int left = len;

    while (total_written < len)
    {
        int written = Write(buf + total_written, left);

        if (written <= 0)
        {
            Disconnect();
            throw Exception("Unable to write data to network.");
        }

        left -= written;
        total_written += written;
    }
}
