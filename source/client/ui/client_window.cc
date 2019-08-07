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

#include "client/ui/client_window.h"
#include "base/logging.h"
#include "client/ui/authorization_dialog.h"
#include "client/ui/client_dialog.h"
#include "client/ui/desktop_window.h"
#include "client/ui/file_manager_window.h"
#include "client/ui/status_dialog.h"
#include "client/client.h"

namespace client {

ClientWindow::ClientWindow(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

ClientWindow::~ClientWindow() = default;

// static
ClientWindow* ClientWindow::create(const ConnectData& connect_data, QWidget* parent)
{
    ClientWindow* window = nullptr;

    switch (connect_data.session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            window = new DesktopWindow(connect_data, parent);
            break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
            window = new FileManagerWindow(connect_data, parent);
            break;

        default:
            break;
    }

    if (window)
        window->setAttribute(Qt::WA_DeleteOnClose);

    return window;
}

bool ClientWindow::connectToHost(const ConnectData* connect_data, QWidget* parent)
{
    ConnectData data;

    if (!connect_data)
    {
        ClientDialog dialog(parent);
        if (dialog.exec() != ClientDialog::Accepted)
            return false;

        data = dialog.connectData();
    }
    else
    {
        data = *connect_data;
    }

    if (data.username.isEmpty() || data.password.isEmpty())
    {
        AuthorizationDialog auth_dialog(parent);

        auth_dialog.setUserName(data.username);
        auth_dialog.setPassword(data.password);

        if (auth_dialog.exec() == AuthorizationDialog::Rejected)
            return false;

        data.username = auth_dialog.userName();
        data.password = auth_dialog.password();
    }

    ClientWindow* window = create(data, parent);
    if (!window)
        return false;

    window->startSession();
    return true;
}

void ClientWindow::startSession()
{
    DCHECK(client_) << "createClient() must be called first.";

    status_dialog_ = new StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);

    // After closing the status dialog, close the session window.
    connect(status_dialog_, &StatusDialog::finished, this, &ClientWindow::close);

    // Show status dialog.
    status_dialog_->show();
    status_dialog_->activateWindow();

    status_dialog_->addStatus(tr("Attempt to connect to %1:%2.")
                              .arg(client_->connectData().address)
                              .arg(client_->connectData().port));

    connect(client_, &Client::started, [this]()
    {
        // Add a message about the successful launch of the session and hide the status dialog.
        status_dialog_->addStatus(tr("Session started."));
        status_dialog_->hide();

        sessionStarted();

        // Show session window.
        show();
        activateWindow();
    });

    connect(client_, &Client::errorOccurred, [this](const QString& message)
    {
        // Hide session window.
        hide();

        sessionError();

        // Display an error in the dialog.
        status_dialog_->addStatus(message);
        status_dialog_->show();
        status_dialog_->activateWindow();
    });

    client_->start();
}

void ClientWindow::sessionStarted()
{
    // Nothing
}

void ClientWindow::sessionError()
{
    // Nothing
}

} // namespace client
