//
// PROJECT:         Aspia
// FILE:            client/file_transfer_downloader.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
#define _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H

#include <QQueue>

#include "client/file_transfer.h"
#include "client/file_transfer_task.h"

namespace aspia {

class FileTransferQueueBuilder;

class FileTransferDownloader : public FileTransfer
{
    Q_OBJECT

public:
    explicit FileTransferDownloader(QObject* parent = nullptr);
    ~FileTransferDownloader();

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
    FileTransferQueueBuilder* builder_;
    QQueue<FileTransferTask> tasks_;

    Q_DISABLE_COPY(FileTransferDownloader)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__FILE_TRANSFER_DOWNLOADER_H
