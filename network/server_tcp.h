//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/server_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SERVER_TCP_H
#define _ASPIA_NETWORK__SERVER_TCP_H

#include <functional>
#include <memory>
#include <list>

#include "base/macros.h"
#include "base/thread.h"
#include "network/scoped_firewall_rule.h"
#include "network/socket_tcp.h"

namespace aspia {

template <class T>
class ServerTCP : public Thread
{
public:
    ServerTCP(uint16_t port, const typename T::EventCallback& event_callback) :
        event_callback_(event_callback),
        port_(port)
    {
        Start();
    }

    ~ServerTCP()
    {
        Stop();
        Wait();
    }

private:
    void RemoveDeadClients()
    {
        // Блокируем список подключенных клиентов.
        AutoLock lock(client_list_lock_);

        auto iter = client_list_.begin();

        // Проходим по всему списку подключенных клиентов.
        while (iter != client_list_.end())
        {
            // Если поток клиента находится в стадии завершения.
            if (iter->get()->IsDead())
            {
                // Удаляем клиента из списка и получаем следующий элемент списка.
                iter = client_list_.erase(iter);
            }
            else
            {
                // Переходим к следующему элементу.
                ++iter;
            }
        }
    }

    void OnClientEvent(typename T::SessionEvent event)
    {
        // Если поток завершает работу, то игнорируем события.
        if (IsTerminating())
            return;

        if (event == typename T::SessionEvent::CLOSE)
            RemoveDeadClients();

        event_callback_(event);
    }

    void OnStop() override
    {
        if (sock_)
        {
            // Разрываем соединение.
            sock_->Disconnect();
        }
    }

    void Worker() override
    {
        firewall_rule_.reset(new ScopedFirewallRule(port_));
        sock_.reset(SocketTCP::Create());

        if (!sock_ || !sock_->Bind(port_))
            return;

        while (!IsTerminating())
        {
            std::unique_ptr<Socket> sock(sock_->Accept());

            if (!sock)
                break;

            T::EventCallback event_callback =
                std::bind(&ServerTCP::OnClientEvent, this, std::placeholders::_1);

            std::unique_ptr<T> client(new T(std::move(sock), event_callback));

            // Блокируем список подключенных клиентов.
            AutoLock lock(client_list_lock_);

            // Добавляем клиента в список подключенных клиентов.
            client_list_.push_back(std::move(client));
        }
    }

private:
    uint16_t port_;

    std::unique_ptr<ScopedFirewallRule> firewall_rule_;
    std::unique_ptr<SocketTCP> sock_;

    std::list<std::unique_ptr<T>> client_list_;
    Lock client_list_lock_;

    typename T::EventCallback event_callback_;

    DISALLOW_COPY_AND_ASSIGN(ServerTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SERVER_TCP_H
