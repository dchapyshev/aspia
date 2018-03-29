//
// PROJECT:         Aspia
// FILE:            client/file_transfer_uploader.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H

#include <QQueue>
#include <QPointer>
#include <memory>

#include "client/file_transfer.h"
#include "client/file_transfer_task.h"

namespace aspia {

class FileTransferQueueBuilder;
class FilePacketizer;

class FileTransferUploader : public FileTransfer
{
    Q_OBJECT

public:
    explicit FileTransferUploader(QObject* parent = nullptr);
    ~FileTransferUploader();

public slots:
    // FileTransfer implementation.
    void start(const QString& source_path,
               const QString& target_path,
               const QList<QPair<QString, bool>>& items) override;
    void applyAction(Action action) override;

private slots:
    void reply(const proto::file_transfer::Request& request,
               const proto::file_transfer::Reply& reply);
    void taskQueueError(const QString& message);
    void taskQueueReady();

private:
    void processNextTask();
    void processCreateDirectoryReply(const proto::file_transfer::CreateDirectoryRequest& request,
                                     proto::file_transfer::Status status);
    void processUploadReply(const proto::file_transfer::UploadRequest& request,
                            proto::file_transfer::Status status);
    void processPacketReply(const proto::file_transfer::Packet& packet,
                            proto::file_transfer::Status status);

    FileTransferQueueBuilder* builder_;
    QQueue<FileTransferTask> tasks_;

    std::unique_ptr<FilePacketizer> packetizer_;

    Q_DISABLE_COPY(FileTransferUploader)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_UPLOADER_H
