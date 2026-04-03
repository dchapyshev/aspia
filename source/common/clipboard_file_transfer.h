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

#ifndef COMMON_CLIPBOARD_FILE_TRANSFER_H
#define COMMON_CLIPBOARD_FILE_TRANSFER_H

#include <QFile>
#include <QObject>

#include <array>
#include <map>
#include <memory>

#include "base/serialization.h"
#include "common/clipboard.h"
#include "proto/desktop_file.h"

namespace common {

class ClipboardFileTransfer final : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardFileTransfer(QObject* parent = nullptr);
    ~ClipboardFileTransfer() final;

    // Sender side: store local file list for later reading.
    void setLocalFileList(const QVector<LocalFileEntry>& files);

    // Handle incoming message from the other side.
    void onIncomingMessage(const QByteArray& buffer);

    // Receiver side: request file data for a specific file index.
    void requestFileData(int file_index);

signals:
    // Serialized proto::file::Message ready to send over the network.
    void sig_sendMessage(const QByteArray& buffer);

    // Emitted when file data chunk is available for delivery to FileStream.
    void sig_fileDataChunk(int file_index, const QByteArray& data, bool is_last);

private:
    // Each chunk is 256 KB. The sender keeps up to kWindowSize chunks "in flight" (sent but not yet
    // acknowledged by the receiver). When the receiver processes a chunk, it sends an ack (Request
    // with the same transfer_id). Each ack decrements in_flight and allows the sender to push the
    // next chunk. This limits the amount of data sitting in send buffers to kWindowSize * kChunkSize
    // (2 MB by default), preventing the send queue from growing unbounded on slow connections while
    // still keeping the pipe full enough to achieve high throughput.
    static constexpr int kChunkSize = 256 * 1024; // 256 KB
    static constexpr int kWindowSize = 8;

    struct OutgoingTransfer
    {
        quint64 transfer_id = 0;
        int file_index = 0;
        std::unique_ptr<QFile> file;
        int in_flight = 0;      // Number of sent chunks waiting for ack.
        bool eof_reached = false; // True after the last chunk has been sent.
    };

    void onFileDataRequest(const proto::file::Request& request);
    void onFileDataReceived(const proto::file::Data& data);
    void onFileDataCancel(const proto::file::Cancel& cancel);
    void onFileDataError(const proto::file::Error& error);

    // Sends chunks until the window is full (in_flight == kWindowSize) or EOF is reached.
    void fillWindow(quint64 transfer_id);

    // Reads one chunk from the file and sends it. Returns true if more data remains, false on EOF.
    bool sendNextChunk(quint64 transfer_id);

    void sendMessage();
    quint64 nextTransferId();

    QVector<LocalFileEntry> local_files_;
    std::map<quint64, OutgoingTransfer> outgoing_transfers_;
    quint64 next_transfer_id_ = 1;
    std::array<char, kChunkSize> read_buffer_;
    base::Parser<proto::file::Message> incoming_message_;
    base::Serializer<proto::file::Message> outgoing_message_;

    Q_DISABLE_COPY_MOVE(ClipboardFileTransfer)
};

} // namespace common

#endif // COMMON_CLIPBOARD_FILE_TRANSFER_H
