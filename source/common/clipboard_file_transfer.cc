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
void ClipboardFileTransfer::setLocalFileList(const QVector<LocalFileEntry>& files)
{
    outgoing_transfers_.clear();
    local_files_ = files;
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onIncomingMessage(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Unable to parse file message";
        return;
    }

    const proto::file::Message& message = incoming_message_.message();

    if (message.has_request())
        onFileDataRequest(message.request());
    else if (message.has_data())
        onFileDataReceived(message.data());
    else if (message.has_cancel())
        onFileDataCancel(message.cancel());
    else if (message.has_error())
        onFileDataError(message.error());
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataRequest(const proto::file::Request& request)
{
    quint64 transfer_id = request.transfer_id();
    int file_index = request.file_index();

    auto it = outgoing_transfers_.find(transfer_id);
    if (it != outgoing_transfers_.end())
    {
        // Ack from the receiver: they consumed one chunk, so we decrement in_flight.
        // If the file has been fully read (eof_reached) and all acks have arrived (in_flight == 0),
        // the transfer is complete and can be cleaned up. Otherwise, fillWindow() will push the
        // next chunk to keep the sliding window full.
        OutgoingTransfer& transfer = it->second;
        --transfer.in_flight;

        if (transfer.eof_reached && transfer.in_flight <= 0)
        {
            outgoing_transfers_.erase(it);
        }
        else
        {
            fillWindow(transfer_id);
        }
        return;
    }

    // Initial request - open the file and fill the window with up to kWindowSize chunks.
    if (file_index < 0 || file_index >= local_files_.size())
    {
        LOG(ERROR) << "Invalid file_index:" << file_index;

        proto::file::Error* error = outgoing_message_.newMessage().mutable_error();
        error->set_transfer_id(transfer_id);
        error->set_file_index(file_index);
        error->set_error_id("invalid_file_index");
        sendMessage();
        return;
    }

    const LocalFileEntry& file_entry = local_files_[file_index];

    if (file_entry.is_dir)
    {
        // Directories have no content to send.
        proto::file::Data* data = outgoing_message_.newMessage().mutable_data();
        data->set_transfer_id(transfer_id);
        data->set_file_index(file_index);
        data->set_is_last(true);
        sendMessage();
        return;
    }

    QString path = file_entry.path;
    auto file = std::make_unique<QFile>(path);

    if (!file->open(QFile::ReadOnly))
    {
        LOG(ERROR) << "Failed to open file:" << path;

        proto::file::Error* error = outgoing_message_.newMessage().mutable_error();
        error->set_transfer_id(transfer_id);
        error->set_file_index(file_index);
        error->set_error_id("open_failed");
        sendMessage();
        return;
    }

    if (request.offset() > 0)
        file->seek(static_cast<qint64>(request.offset()));

    OutgoingTransfer transfer;
    transfer.transfer_id = transfer_id;
    transfer.file_index = file_index;
    transfer.file = std::move(file);

    outgoing_transfers_[transfer_id] = std::move(transfer);
    fillWindow(transfer_id);
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataCancel(const proto::file::Cancel& cancel)
{
    LOG(INFO) << "Transfer cancelled:" << cancel;
    outgoing_transfers_.erase(cancel.transfer_id());
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataError(const proto::file::Error& error)
{
    LOG(ERROR) << "File transfer error:" << error;

    outgoing_transfers_.erase(error.transfer_id());

    // Terminate the stream for the affected file so Read() unblocks.
    emit sig_fileDataChunk(error.file_index(), QByteArray(), true);
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::requestFileData(int file_index)
{
    proto::file::Request* request = outgoing_message_.newMessage().mutable_request();
    request->set_transfer_id(nextTransferId());
    request->set_file_index(file_index);
    request->set_offset(0);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::onFileDataReceived(const proto::file::Data& data)
{
    QByteArray chunk = QByteArray::fromStdString(data.data());
    emit sig_fileDataChunk(data.file_index(), chunk, data.is_last());

    if (!data.is_last())
    {
        // Send ack (Request with the same transfer_id) back to the sender. The sender treats this
        // as confirmation that one chunk has been consumed, decrements in_flight, and sends the
        // next chunk from the file. No ack is sent for the last chunk because the sender already
        // knows the transfer is finishing and will clean up after all in-flight acks arrive.
        proto::file::Request* ack = outgoing_message_.newMessage().mutable_request();
        ack->set_transfer_id(data.transfer_id());
        ack->set_file_index(data.file_index());
        sendMessage();
    }
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::fillWindow(quint64 transfer_id)
{
    auto it = outgoing_transfers_.find(transfer_id);
    if (it == outgoing_transfers_.end())
        return;

    OutgoingTransfer& transfer = it->second;

    while (transfer.in_flight < kWindowSize && !transfer.eof_reached)
    {
        if (!sendNextChunk(transfer_id))
            break;
    }
}

//--------------------------------------------------------------------------------------------------
bool ClipboardFileTransfer::sendNextChunk(quint64 transfer_id)
{
    auto it = outgoing_transfers_.find(transfer_id);
    if (it == outgoing_transfers_.end())
        return false;

    OutgoingTransfer& transfer = it->second;

    qint64 bytes_read = transfer.file->read(read_buffer_.data(), kChunkSize);
    bool at_end = (bytes_read <= 0) || transfer.file->atEnd();

    proto::file::Data* data = outgoing_message_.newMessage().mutable_data();
    data->set_transfer_id(transfer_id);
    data->set_file_index(transfer.file_index);
    if (bytes_read > 0)
        data->set_data(read_buffer_.data(), bytes_read);
    data->set_is_last(at_end);
    sendMessage();

    if (at_end)
    {
        transfer.eof_reached = true;
        if (transfer.in_flight == 0)
            outgoing_transfers_.erase(it);
        return false;
    }

    ++transfer.in_flight;
    return true;
}

//--------------------------------------------------------------------------------------------------
void ClipboardFileTransfer::sendMessage()
{
    emit sig_sendMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
quint64 ClipboardFileTransfer::nextTransferId()
{
    return next_transfer_id_++;
}

} // namespace common
