//
// PROJECT:         Aspia
// FILE:            client/file_transfer_downloader.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_downloader.h"

#include "client/file_transfer_queue_builder.h"

namespace aspia {

FileTransferDownloader::FileTransferDownloader(QObject* parent)
    : FileTransfer(parent)
{
    // Nothing
}

FileTransferDownloader::~FileTransferDownloader() = default;

void FileTransferDownloader::start(const QString& source_path,
                                   const QString& target_path,
                                   const QList<QPair<QString, bool>>& items)
{
    builder_ = new FileTransferQueueBuilder();

    connect(builder_, &FileTransferQueueBuilder::started,
            this, &FileTransferDownloader::started);

    connect(builder_, &FileTransferQueueBuilder::error,
            this, &FileTransferDownloader::taskQueueError);

    connect(builder_, &FileTransferQueueBuilder::finished,
            this, &FileTransferDownloader::taskQueueReady);

    connect(builder_, &FileTransferQueueBuilder::finished,
            builder_, &FileTransferQueueBuilder::deleteLater);

    connect(builder_, &FileTransferQueueBuilder::request,
            this, &FileTransferDownloader::remoteRequest);

    builder_->start(source_path, target_path, items);
}

void FileTransferDownloader::applyAction(Action action)
{

}

void FileTransferDownloader::reply(const proto::file_transfer::Request& request,
                                   const proto::file_transfer::Reply& reply)
{

}

void FileTransferDownloader::taskQueueError(const QString& message)
{

}

void FileTransferDownloader::taskQueueReady()
{
    Q_ASSERT(builder_ != nullptr);

    tasks_ = builder_->taskQueue();
}

} // namespace aspia
