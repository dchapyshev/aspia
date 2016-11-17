/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/server.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SERVER__SERVER_H
#define _ASPIA_SERVER__SERVER_H

#include "aspia_config.h"

#include <functional>
#include <future>
#include <memory>
#include <list>

#include "base/thread.h"
#include "base/exception.h"
#include "base/mutex.h"
#include "base/macros.h"
#include "network/socket_tcp.h"
#include "server/client.h"

class Server : public Thread
{
public:
    Server(const char *address, int port);
    ~Server();

    typedef std::function<void(uint32_t, const std::string&)> OnClientConnectedCallback;
    typedef std::function<void(const std::string&)> OnClientRejectedCallback;
    typedef std::function<void(uint32_t)> OnClientDisconnectedCallback;

    void RegisterCallbacks(OnClientConnectedCallback OnClientConnected,
                           OnClientRejectedCallback OnClientRejected,
                           OnClientDisconnectedCallback OnClientDisconnected);

    void DisconnectAll();
    void Disconnect(uint32_t client_id);

    // Данный метод вызывается клиентом при отключении.
    void RemoteDeadClients();

private:
    class ClientIdGenerator
    {
    public:
        ClientIdGenerator() : curr_id_(0) {}
        ~ClientIdGenerator() {}

        uint32_t Generate() { return curr_id_++; }

    private:
        uint32_t curr_id_;
    };

    void Worker() override;
    void OnStart() override;
    void OnStop() override;

private:
    OnClientConnectedCallback OnClientConnected_;
    OnClientRejectedCallback OnClientRejected_;
    OnClientDisconnectedCallback OnClientDisconnected_;

    std::unique_ptr<Socket> socket_;

    ClientIdGenerator id_generator_;
    std::list<std::unique_ptr<Client>> client_list_;
    Mutex client_list_lock_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

#endif // _ASPIA_SERVER__SERVER_H
