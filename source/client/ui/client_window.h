//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__CLIENT_WINDOW_H
#define CLIENT__UI__CLIENT_WINDOW_H

#include <QWidget>

#include "base/macros_magic.h"
#include "client/connect_data.h"

namespace client {

class Client;
class StatusDialog;

class ClientWindow : public QWidget
{
    Q_OBJECT

public:
    virtual ~ClientWindow();

    // Creates a client window.
    // |connect_data| must contain valid data to connect.
    // Parameter |parent| is used to specify the parent window. If you do not need to specify the
    // parent window, then the parameter must be equal to nullptr.
    static ClientWindow* create(const ConnectData& connect_data, QWidget* parent = nullptr);

    // Connects to a host.
    // If |connect_data| is not zero, then the data is used to connect.
    // If the parameter is equal to zero, then a dialog for entering parameters will be displayed.
    // If the username and/or password are not specified in the connection parameters, the
    // authorization dialog will be displayed.
    // Parameter |parent| is used to specify the parent window. If you do not need to specify the
    // parent window, then the parameter must be equal to nullptr.
    static bool connectToHost(const ConnectData* connect_data = nullptr,
                              QWidget* parent = nullptr);

    // Starts a client session.
    void startSession();

protected:
    explicit ClientWindow(QWidget* parent);

    // Called when the client session is running (network connection is established) and the
    // client is ready to receive or send data.
    virtual void sessionStarted();

    // Called when the client session is complete.
    virtual void sessionFinished();

    // Creates a client.
    // Each child must call this method in the constructor to create the client.
    template <class ClassType, class ... Args>
    void createClient(Args ... args)
    {
        client_ = new ClassType(args..., this);
    }

    // Returns the current client.
    Client* currentClient() { return client_; }

private:
    StatusDialog* status_dialog_ = nullptr;
    Client* client_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ClientWindow);
};

} // namespace client

#endif // CLIENT__UI__CLIENT_WINDOW_H
