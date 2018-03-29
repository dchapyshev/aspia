//
// PROJECT:         Aspia
// FILE:            client/client_session_file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_file_transfer.h"

#include <QDebug>
#include <QMetaType>
#include <QThread>

#include "client/ui/file_manager_window.h"
#include "client/file_request.h"
#include "client/file_worker.h"
#include "protocol/message_serialization.h"

namespace aspia {

Q_DECLARE_METATYPE(FileReplyReceiver);

ClientSessionFileTransfer::ClientSessionFileTransfer(proto::Computer* computer, QObject* parent)
    : ClientSession(parent),
      computer_(computer)
{
    qRegisterMetaType<proto::file_transfer::Request>();
    qRegisterMetaType<proto::file_transfer::Reply>();
    qRegisterMetaType<FileReplyReceiver>();
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    if (worker_thread_)
    {
        worker_thread_->quit();
        worker_thread_->wait();
    }

    delete file_manager_;
}

void ClientSessionFileTransfer::readMessage(const QByteArray& buffer)
{
    proto::file_transfer::Reply reply;

    if (!ParseMessage(buffer, reply))
    {
        emit sessionError(tr("Session error: Invalid message from host."));
        return;
    }

    const proto::file_transfer::Request& request = tasks_.front().first;
    const FileReplyReceiver& receiver = tasks_.front().second;

    QMetaObject::invokeMethod(receiver.object,
                              receiver.slot,
                              Qt::DirectConnection,
                              Q_ARG(const proto::file_transfer::Request&, request),
                              Q_ARG(const proto::file_transfer::Reply&, reply));

    tasks_.pop_front();
}

void ClientSessionFileTransfer::startSession()
{
    worker_thread_ = new QThread(this);
    worker_ = new FileWorker();
    worker_->moveToThread(worker_thread_);
    worker_thread_->start();

    file_manager_ = new FileManagerWindow(computer_);

    // When the window is closed, we close the session.
    connect(file_manager_, &FileManagerWindow::windowClose,
            this, &ClientSessionFileTransfer::sessionClosed);

    connect(file_manager_, &FileManagerWindow::localRequest,
            worker_, &FileWorker::executeRequest);

    connect(file_manager_, &FileManagerWindow::remoteRequest,
            this, &ClientSessionFileTransfer::remoteRequest);

    file_manager_->show();
    file_manager_->activateWindow();
    file_manager_->refresh();
}

void ClientSessionFileTransfer::closeSession()
{
    // If the end of the session is not initiated by the user, then we do not send the session
    // end signal.
    disconnect(file_manager_, &FileManagerWindow::windowClose,
               this, &ClientSessionFileTransfer::sessionClosed);
    file_manager_->close();
}

void ClientSessionFileTransfer::remoteRequest(const proto::file_transfer::Request& request,
                                              const FileReplyReceiver& receiver)
{
    tasks_.push_back(QPair<proto::file_transfer::Request, FileReplyReceiver>(request, receiver));
    emit sessionMessage(SerializeMessage(request));
}

} // namespace aspia
