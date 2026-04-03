//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/clipboard_file_transfer.h"

#include <QTimer>

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
ClipboardFileTransfer::ClipboardFileTransfer(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClipboardFileTransfer::~ClipboardFileTransfer()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::setLocalFileList(const proto::clipboard::Event::FileList& files)
{
    outgoing_transfers_.clear();
    local_files_ = files;
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataRequest(const proto::file::Request& request)
{
    uint64_t transfer_id = request.transfer_id();
    int file_index = request.file_index();

    if (file_index < 0 || file_index >= local_files_.file_size())
    {
        LOG(ERROR) << "Invalid file_index:" << file_index;

        proto::file::Error error;
        error.set_transfer_id(transfer_id);
        error.set_file_index(file_index);
        error.set_error_id("invalid_file_index");
        emit sig_sendError(error);
        return;
    }

    const auto& file_entry = local_files_.file(file_index);

    if (file_entry.is_dir())
    {
        // Directories have no content to send.
        proto::file::Data data;
        data.set_transfer_id(transfer_id);
        data.set_file_index(file_index);
        data.set_is_last(true);
        emit sig_sendData(data);
        return;
    }

    QString path = QString::fromStdString(file_entry.path());
    auto file = std::make_unique<QFile>(path);

    if (!file->open(QFile::ReadOnly))
    {
        LOG(ERROR) << "Failed to open file:" << path;

        proto::file::Error error;
        error.set_transfer_id(transfer_id);
        error.set_file_index(file_index);
        error.set_error_id("open_failed");
        emit sig_sendError(error);
        return;
    }

    if (request.offset() > 0)
        file->seek(static_cast<qint64>(request.offset()));

    OutgoingTransfer transfer;
    transfer.transfer_id = transfer_id;
    transfer.file_index = file_index;
    transfer.file = std::move(file);

    outgoing_transfers_[transfer_id] = std::move(transfer);
    sendNextChunk(transfer_id);
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataCancel(const proto::file::Cancel& cancel)
{
    LOG(INFO) << "Transfer cancelled:" << cancel.transfer_id();
    outgoing_transfers_.remove(cancel.transfer_id());
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataError(const proto::file::Error& error)
{
    LOG(ERROR) << "File transfer error:" << QString::fromStdString(error.error_id())
               << "transfer:" << error.transfer_id()
               << "file:" << error.file_index();

    outgoing_transfers_.remove(error.transfer_id());

    // Terminate the stream for the affected file so Read() unblocks.
    emit sig_fileDataChunk(error.file_index(), QByteArray(), true);
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::requestFileData(int file_index)
{
    uint64_t transfer_id = nextTransferId();

    proto::file::Request request;
    request.set_transfer_id(transfer_id);
    request.set_file_index(file_index);
    request.set_offset(0);

    emit sig_sendRequest(request);
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataReceived(const proto::file::Data& data)
{
    QByteArray chunk = QByteArray::fromStdString(data.data());
    emit sig_fileDataChunk(data.file_index(), chunk, data.is_last());
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::sendNextChunk(uint64_t transfer_id)
{
    auto it = outgoing_transfers_.find(transfer_id);
    if (it == outgoing_transfers_.end())
        return;

    OutgoingTransfer& transfer = it.value();

    QByteArray buffer = transfer.file->read(kChunkSize);
    bool at_end = transfer.file->atEnd() || buffer.isEmpty();

    proto::file::Data data;
    data.set_transfer_id(transfer_id);
    data.set_file_index(transfer.file_index);
    data.set_data(buffer.toStdString());
    data.set_is_last(at_end);

    emit sig_sendData(data);

    if (at_end)
    {
        outgoing_transfers_.erase(it);
    }
    else
    {
        QTimer::singleShot(0, this, [this, transfer_id]()
        {
            sendNextChunk(transfer_id);
        });
    }
}

//--------------------------------------------------------------------------------------------------
uint64_t ClipboardFileTransfer::nextTransferId()
{
    return next_transfer_id_.fetch_add(1);
}

} // namespace common
