//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/client_session_file_transfer.h"

#include <QMetaType>
#include <QThread>

#include "client/ui/file_manager_window.h"
#include "common/file_request.h"
#include "common/file_worker.h"
#include "common/message_serialization.h"

namespace aspia {

ClientSessionFileTransfer::ClientSessionFileTransfer(ConnectData* connect_data, QObject* parent)
    : ClientSession(parent),
      connect_data_(connect_data)
{
    qRegisterMetaType<proto::file_transfer::Request>();
    qRegisterMetaType<proto::file_transfer::Reply>();
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    if (worker_thread_)
    {
        worker_thread_->quit();
        worker_thread_->wait();
    }

    for (auto request : requests_)
        delete request;
    requests_.clear();

    delete worker_;
}

void ClientSessionFileTransfer::messageReceived(const QByteArray& buffer)
{
    proto::file_transfer::Reply reply;

    if (!parseMessage(buffer, reply))
    {
        emit errorOccurred(tr("Session error: Invalid message from host."));
        return;
    }

    if (reply.status() == proto::file_transfer::STATUS_NO_LOGGED_ON_USER)
    {
        emit errorOccurred(
            tr("Session error: There are no logged in users. File transfer is not available."));
        return;
    }

    FileRequest* request = requests_.front();
    requests_.pop_front();

    request->sendReply(reply);
    delete request;
}

void ClientSessionFileTransfer::startSession()
{
    worker_thread_ = new QThread(this);
    worker_ = new FileWorker();
    worker_->moveToThread(worker_thread_);
    worker_thread_->start();

    file_manager_ = new FileManagerWindow(connect_data_);

    // When the window is closed, we close the session.
    connect(file_manager_, &FileManagerWindow::windowClose,
            this, &ClientSessionFileTransfer::closedByUser);

    connect(file_manager_, &FileManagerWindow::windowClose,
            file_manager_, &FileManagerWindow::deleteLater);

    connect(file_manager_, &FileManagerWindow::localRequest,
            worker_, &FileWorker::executeRequest);

    connect(file_manager_, &FileManagerWindow::remoteRequest,
            this, &ClientSessionFileTransfer::remoteRequest);

    file_manager_->show();
    file_manager_->activateWindow();
    file_manager_->refresh();

    emit started();
}

void ClientSessionFileTransfer::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(file_manager_, &FileManagerWindow::windowClose,
               this, &ClientSessionFileTransfer::closedByUser);
    file_manager_->close();
}

void ClientSessionFileTransfer::remoteRequest(FileRequest* request)
{
    requests_.push_back(QPointer<FileRequest>(request));
    emit sendMessage(serializeMessage(request->request()));
}

} // namespace aspia
