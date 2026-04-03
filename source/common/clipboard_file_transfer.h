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

#include <atomic>
#include <map>
#include <memory>

#include "proto/desktop_clipboard.h"
#include "proto/desktop_file.h"

namespace common {

class ClipboardFileTransfer final : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardFileTransfer(QObject* parent = nullptr);
    ~ClipboardFileTransfer() final;

    // Sender side: store local file list for later reading.
    void setLocalFileList(const proto::clipboard::Event::FileList& files);

    // Sender side: handle incoming request for file data.
    void onFileDataRequest(const proto::file::Request& request);

    // Sender side: handle cancellation from receiver.
    void onFileDataCancel(const proto::file::Cancel& cancel);

    // Sender side: handle error from receiver.
    void onFileDataError(const proto::file::Error& error);

    // Receiver side: request file data for a specific file index.
    void requestFileData(int file_index);

    // Receiver side: handle incoming file data.
    void onFileDataReceived(const proto::file::Data& data);

signals:
    void sig_sendRequest(const proto::file::Request& request);
    void sig_sendData(const proto::file::Data& data);
    void sig_sendCancel(const proto::file::Cancel& cancel);
    void sig_sendError(const proto::file::Error& error);

    // Emitted when file data chunk is available for delivery to FileStream.
    void sig_fileDataChunk(int file_index, const QByteArray& data, bool is_last);

private:
    static constexpr int kChunkSize = 256 * 1024; // 256 KB

    struct OutgoingTransfer
    {
        quint64 transfer_id = 0;
        int file_index = 0;
        std::unique_ptr<QFile> file;
    };

    void sendNextChunk(quint64 transfer_id);
    quint64 nextTransferId();

    proto::clipboard::Event::FileList local_files_;
    std::map<quint64, OutgoingTransfer> outgoing_transfers_;
    std::atomic<quint64> next_transfer_id_{ 1 };

    Q_DISABLE_COPY_MOVE(ClipboardFileTransfer)
};

} // namespace common

#endif // COMMON_CLIPBOARD_FILE_TRANSFER_H
