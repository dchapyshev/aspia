//
// PROJECT:         Aspia
// FILE:            client/file_worker.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_WORKER_H
#define _ASPIA_CLIENT__FILE_WORKER_H

#include <QObject>

#include "client/file_reply_receiver.h"
#include "proto/file_transfer_session.pb.h"

namespace aspia {

class FileWorker : public QObject
{
    Q_OBJECT

public:
    FileWorker(QObject* parent = nullptr);
    ~FileWorker() = default;

public slots:
    void executeRequest(const proto::file_transfer::Request& request,
                        const FileReplyReceiver& receiver);

private:
    proto::file_transfer::Reply doRequest(const proto::file_transfer::Request& request);

    Q_DISABLE_COPY(FileWorker)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_WORKER_H
