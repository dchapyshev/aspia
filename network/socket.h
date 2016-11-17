/*
* PROJECT:         Aspia Remote Desktop
* FILE:            network/socket.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_NETWORK__SOCKET_H
#define _ASPIA_NETWORK__SOCKET_H

#include "aspia_config.h"

#include <string>
#include <stdint.h>
#include <memory>

#include "base/exception.h"
#include "base/macros.h"
#include "base/mutex.h"

class Socket
{
public:
    Socket();
    virtual ~Socket();

    virtual void Connect(const char *hostname, int port) = 0;
    virtual int Write(const char *buf, int len) = 0;
    virtual int Read(char *buf, int len) = 0;
    virtual void Bind(const char *hostname, int port) = 0;
    virtual void Listen() = 0;
    virtual std::unique_ptr<Socket> Accept() = 0;
    virtual void Close() = 0;
    virtual std::string GetIpAddress() = 0;

    // Sets the timeout, in milliseconds.
    virtual void SetWriteTimeout(int timeout) = 0;

    // Sets the timeout, in milliseconds.
    virtual void SetReadTimeout(int timeout) = 0;

    // Disable or enable Nagle’s algorithm.
    virtual void SetNoDelay(bool enable) = 0;

    static bool IsValidHostName(const char *hostname);
    static bool IsValidPort(int port);

    template<class T>
    void WriteMessage(T *message)
    {
        LockGuard<Mutex> guard(&write_lock_);

        if (!message->SerializeToString(&write_message_))
        {
            Close();
            throw Exception("Unable to serialize the message.");
        }

        uint32_t packet_size = write_message_.size();

        if (!packet_size)
        {
            Close();
            throw Exception("Wrong packet size.");
        }

        //
        // Реализован своеобразный аналог TCP_CORK из Linux.
        // TCP не гарантирует немедленной отправки всего сообщения за один вызов
        // send() и для отправки всего сообщения данный вызов приходится делать
        // некоторое количество раз. По умолчанию для TCP соединения включен
        // алгоритм Нейгла, который задерживает пакеты в очереди на отправку для
        // того, чтобы избежать лишней передачи TCP-заголовков (которые вместе с
        // заголовками IP имеют размер 40 байт).
        // Для минимизации задержек алгоритм Нейгла должен быть отключен.
        // На время отправки одного логического сообщения мы включаем алгоритм
        // Нейгла, чтобы сообщение было отправлено наименьшим количеством TCP-
        // пакетов.
        // После отправки логического сообщения мы включаем алгоритм Нейгла и
        // вызываем send(), чтобы отправить пакеты в очереди алгоритма.
        //

        // Включаем алгоритм Нейгла
        SetNoDelay(true);

        // Отправляем размер данных и сами данные.
        Writer(reinterpret_cast<const char*>(&packet_size), sizeof(uint32_t));
        Writer(write_message_.data(), packet_size);

        // Отключаем алгоритм Нейгла.
        SetNoDelay(false);

        //
        // Вызываем отправку данных и нулевым размером данных для
        // отправки пакетов в очереди.
        //
        Write("", 0);
    }

    template<class T>
    void ReadMessage(std::unique_ptr<T> *message)
    {
        uint32_t packet_size = 0;

        Reader(reinterpret_cast<char*>(&packet_size), sizeof(packet_size));

        if (!packet_size)
        {
            Close();
            throw Exception("Serialized message size is equal to zero.");
        }

        if (read_message_.size() < packet_size)
            read_message_.resize(packet_size);

        Reader(&read_message_[0], packet_size);

        if (!message->get()->ParseFromArray(read_message_.data(), packet_size))
        {
            Close();
            throw Exception("Unable to parse the message.");
        }
    }

private:
    void Reader(char *buf, int len);
    void Writer(const char *buf, int len);
    static bool IsHostnameChar(char c);

private:
    std::string write_message_;
    std::string read_message_;
    Mutex write_lock_;

    DISALLOW_COPY_AND_ASSIGN(Socket);
};

#endif // _ASPIA_NETWORK__SOCKET_H
