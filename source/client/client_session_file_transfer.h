//
// PROJECT:         Aspia
// FILE:            client/client_session_file_transfer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
#define _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H

#include <QQueue>
#include <QPair>

#include "client/client_session.h"
#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"
#include "proto/computer.pb.h"

Q_DECLARE_METATYPE(proto::file_transfer::Request);
Q_DECLARE_METATYPE(proto::file_transfer::Reply);

namespace aspia {

class FileManagerWindow;
class FileWorker;

class ClientSessionFileTransfer : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionFileTransfer(proto::Computer* computer, QObject* parent);
    ~ClientSessionFileTransfer();

public slots:
    // ClientSession implementation.
    void readMessage(const QByteArray& buffer) override;
    void startSession() override;
    void closeSession() override;

private slots:
    void remoteRequest(const proto::file_transfer::Request& request,
                       const FileReplyReceiver& receiver);

private:
    proto::Computer* computer_;
    FileManagerWindow* file_manager_;

    FileWorker* worker_;
    QThread* worker_thread_;

    QQueue<QPair<proto::file_transfer::Request, FileReplyReceiver>> tasks_;

    Q_DISABLE_COPY(ClientSessionFileTransfer)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_FILE_TRANSFER_H
