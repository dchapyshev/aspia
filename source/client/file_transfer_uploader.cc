//
// PROJECT:         Aspia
// FILE:            client/file_transfer_uploader.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_transfer_uploader.h"

#include "client/file_transfer_queue_builder.h"
#include "client/file_status.h"
#include "protocol/file_packetizer.h"

namespace aspia {

FileTransferUploader::FileTransferUploader(QObject* parent)
    : FileTransfer(parent)
{
    // Nothing
}

FileTransferUploader::~FileTransferUploader() = default;

void FileTransferUploader::start(const QString& source_path,
                                 const QString& target_path,
                                 const QList<QPair<QString, bool>>& items)
{
    builder_ = new FileTransferQueueBuilder();

    connect(builder_, &FileTransferQueueBuilder::started,
            this, &FileTransferUploader::started);

    connect(builder_, &FileTransferQueueBuilder::error,
            this, &FileTransferUploader::taskQueueError);

    connect(builder_, &FileTransferQueueBuilder::finished,
            this, &FileTransferUploader::taskQueueReady);

    connect(builder_, &FileTransferQueueBuilder::finished,
            builder_, &FileTransferQueueBuilder::deleteLater);

    connect(builder_, &FileTransferQueueBuilder::request,
            this, &FileTransferUploader::localRequest);

    builder_->start(source_path, target_path, items);
}

void FileTransferUploader::applyAction(Action action)
{

}

void FileTransferUploader::reply(const proto::file_transfer::Request& request,
                                 const proto::file_transfer::Reply& reply)
{
    if (request.has_create_directory_request())
    {
        processCreateDirectoryReply(request.create_directory_request(), reply.status());
    }
    else if (request.has_upload_request())
    {
        processUploadReply(request.upload_request(), reply.status());
    }
    else if (request.has_packet())
    {
        processPacketReply(request.packet(), reply.status());
    }
    else
    {
        emit error(this, Action::Abort, tr("An unexpected response to the request was received"));
    }
}

void FileTransferUploader::taskQueueError(const QString& message)
{

}

void FileTransferUploader::taskQueueReady()
{
    Q_ASSERT(builder_ != nullptr);

    tasks_ = builder_->taskQueue();
}

void FileTransferUploader::processNextTask()
{

}

void FileTransferUploader::processCreateDirectoryReply(
    const proto::file_transfer::CreateDirectoryRequest& request,
    proto::file_transfer::Status status)
{
    if (status == proto::file_transfer::STATUS_SUCCESS ||
        status == proto::file_transfer::STATUS_PATH_ALREADY_EXISTS)
    {
        processNextTask();
        return;
    }

    if (directoryCreateErrorAction() != Action::Ask)
    {
        applyAction(directoryCreateErrorAction());
        return;
    }

    emit error(this, Action::Abort | Action::Skip | Action::SkipAll,
               tr("Failed to create directory \"%1\": %2")
               .arg(QString::fromStdString(request.path()))
               .arg(fileStatusToString(status)));
}

void FileTransferUploader::processUploadReply(const proto::file_transfer::UploadRequest& request,
                                              proto::file_transfer::Status status)
{
    if (status != proto::file_transfer::STATUS_SUCCESS)
    {
        if (status == proto::file_transfer::STATUS_PATH_ALREADY_EXISTS)
        {
            if (fileExistsAction() != Action::Ask)
            {
                applyAction(fileExistsAction());
                return;
            }
        }
        else
        {
            if (fileCreateErrorAction() != Action::Ask)
            {
                applyAction(fileCreateErrorAction());
                return;
            }
        }

        Actions actions = Action::Abort | Action::Skip | Action::SkipAll;

        if (status == proto::file_transfer::STATUS_PATH_ALREADY_EXISTS)
            actions |= Action::Replace | Action::ReplaceAll;

        emit error(this, actions,
                   tr("Failed to create file \"%1\": %2")
                   .arg(QString::fromStdString(request.path()))
                   .arg(fileStatusToString(status)));
        return;
    }

    const FileTransferTask& current_task = tasks_.front();

    packetizer_ = FilePacketizer::Create(current_task.sourcePath());
    if (!packetizer_)
    {
        if (fileOpenErrorAction() != Action::Ask)
        {
            applyAction(fileOpenErrorAction());
            return;
        }

        emit error(this, Action::Abort | Action::Skip | Action::SkipAll,
                   tr("Failed to open file: \"%1\": %2")
                   .arg(current_task.sourcePath()
                   .arg(fileStatusToString(proto::file_transfer::STATUS_ACCESS_DENIED))));
        return;
    }

    std::unique_ptr<proto::file_transfer::Packet> packet =
        packetizer_->CreateNextPacket();
    if (!packet)
    {
        if (fileReadErrorAction() != Action::Ask)
        {
            applyAction(fileReadErrorAction());
            return;
        }

        emit error(this, Action::Abort | Action::Skip | Action::SkipAll,
                   tr("Failed to read file: \"%1\": %2")
                   .arg(current_task.sourcePath()
                   .arg(fileStatusToString(proto::file_transfer::STATUS_ACCESS_DENIED))));
        return;
    }
}

void FileTransferUploader::processPacketReply(const proto::file_transfer::Packet& packet,
                                              proto::file_transfer::Status status)
{
    if (status != proto::file_transfer::STATUS_SUCCESS)
    {

    }
}

} // namespace aspia
