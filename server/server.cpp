/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/server.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "server/server.h"

#include "base/logging.h"

Server::Server(const char *hostname, int port)
{
    // Создаем экземпляр сокета.
    socket_.reset(new SocketTCP());

    // Создаем сервер для входящих подключений.
    socket_->Bind(hostname, port);
    socket_->Listen();
}

Server::~Server()
{
    // Nothing
}

void Server::RegisterCallbacks(OnClientConnectedCallback OnClientConnected,
                               OnClientRejectedCallback OnClientRejected,
                               OnClientDisconnectedCallback OnClientDisconnected)
{
    OnClientConnected_    = OnClientConnected;
    OnClientRejected_     = OnClientRejected;
    OnClientDisconnected_ = OnClientDisconnected;
}

void Server::DisconnectAll()
{
    {
        // Блокируем список подключенных клиентов.
        LockGuard<Mutex> guard(&client_list_lock_);

        // Проходим по всему списку подключенных клиентов.
        for (auto iter = client_list_.begin(); iter != client_list_.end(); ++iter)
        {
            // Даем команду потоку клиента остановиться.
            iter->get()->Stop();
        }
    }

    // Убираем отключившихся клиентов из списка.
    RemoteDeadClients();
}

void Server::Disconnect(uint32_t client_id)
{
    {
        // Блокируем список подключенных клиентов.
        LockGuard<Mutex> guard(&client_list_lock_);

        // Проходим по всему списку подключенных клиентов.
        for (auto iter = client_list_.begin(); iter != client_list_.end(); ++iter)
        {
            Client *client = iter->get();

            // Если ID клиента совпадает с запрошенным.
            if (client->GetID() == client_id)
            {
                // Даем команду остановить поток клиента.
                client->Stop();
                break;
            }
        }
    }

    // Убираем отключившегося клиента из списка.
    RemoteDeadClients();
}

void Server::RemoteDeadClients()
{
    // Блокируем список подключенных клиентов.
    LockGuard<Mutex> guard(&client_list_lock_);

    auto iter = client_list_.begin();

    // Проходим по всему списку подключенных клиентов.
    while (iter != client_list_.end())
    {
        Client *client = iter->get();

        // Если поток клиента находится в стадии завершения.
        if (client->IsEndOfThread())
        {
            // Получаем уникальный ID клиента.
            uint32_t id = client->GetID();

            // Дожидаемся завершения потока.
            client->WaitForEnd();

            // Удаляем клиента из списка и получаем следующий элемент списка.
            iter = client_list_.erase(iter);

            // Если callback был инициализирован.
            if (!OnClientDisconnected_._Empty())
            {
                //
                // Асинхронно вызываем callback для оповещения о том,
                // что клиент отключился.
                //
                std::async(std::launch::async, OnClientDisconnected_, id);
            }
        }
        else
        {
            // Переходим к следующему элементу.
            ++iter;
        }
    }
}

void Server::Worker()
{
    DLOG(INFO) << "Server thread started";

    try
    {
        // Продолжаем цикл пока не будет дана команда остановить текущий поток.
        while (!IsEndOfThread())
        {
            // Принимаем входящее подключение от клиента.
            std::unique_ptr<Socket> client_socket = socket_->Accept();

            // Получаем IP адрес подключенного клиента.
            std::string ip(client_socket->GetIpAddress());

            try
            {
                // Генерируем уникальный идентификатор клиента.
                uint32_t id = id_generator_.Generate();

                Client::OnDisconnectedCallback on_disconnected =
                    std::bind(&Server::Disconnect, this, std::placeholders::_1);

                //
                // Инициализируем нового подключенного клиента. Если во время инициализации
                // возникнет исключение, то данный клиент отклонен из-за ошибки инициализации
                // или из-за неправильной авторизации.
                //
                std::unique_ptr<Client> client(new Client(std::move(client_socket),
                                                          id,
                                                          on_disconnected));

                // Запускаем поток клиента.
                client->Start();

                // Блокируем список подключенных клиентов.
                LockGuard<Mutex> guard(&client_list_lock_);

                // Добавляем клиента в список подключенных клиентов.
                client_list_.push_back(std::move(client));

                // Если callback был инициализирован.
                if (!OnClientConnected_._Empty())
                {
                    // Асинхронно вызываем callback для уведомления о том, что подключился новый клиент.
                    std::async(std::launch::async, OnClientConnected_, id, ip);
                }
            }
            catch (const Exception &client_err)
            {
                LOG(ERROR) << "Exception when a new client connection: " << client_err.What();

                // Если callback был инициализирован.
                if (!OnClientRejected_._Empty())
                {
                    // Асинхронно вызываем callback для уведомления о том, что клиент отклонен.
                    std::async(std::launch::async, OnClientRejected_, ip);
                }
            }
        }
    }
    catch (const Exception &server_err)
    {
        //
        // Если возникло исключение при выполнении сервера, то записываем событие в
        // лог и выходим из потока.
        //
        LOG(ERROR) << "Exception in server thread: " << server_err.What();
    }

    DLOG(INFO) << "Server thread stopped";
}

void Server::OnStop()
{
    // Отключаем всех подключенных клиентов.
    DisconnectAll();
}
