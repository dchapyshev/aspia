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

#ifndef COMMON_MAC_FILE_PROMISE_WRITER_H
#define COMMON_MAC_FILE_PROMISE_WRITER_H

#include <QByteArray>
#include <QFile>

#include <condition_variable>
#include <mutex>

namespace common {

// macOS analogue of FileStream (Windows IStream).
//
// When Finder pastes files from NSFilePromiseProvider, it calls the delegate's
// writePromiseTo:completionHandler: on an NSOperationQueue thread. That thread calls
// writeToDestination(), which blocks until all data has been received (or the transfer
// is terminated). Data arrives from the Qt thread via addData(). The synchronization
// uses a mutex + condition_variable, mirroring the Windows Event-based approach in
// FileStream.
class FilePromiseWriter final
{
public:
    explicit FilePromiseWriter(qint64 file_size = 0);
    ~FilePromiseWriter();

    // Called from the Qt thread (via ClipboardMac::addFileData).
    void addData(const QByteArray& data, bool is_last);
    void terminate();

    // Called from the NSOperationQueue thread. Blocks until all data is written or
    // the transfer is terminated. Returns true on success.
    bool writeToDestination(const QString& dest_path);

private:
    qint64 file_size_ = 0;

    std::mutex mutex_;
    std::condition_variable cv_;
    QByteArray buffer_;
    bool is_finished_ = false;
    bool is_terminated_ = false;

    Q_DISABLE_COPY_MOVE(FilePromiseWriter)
};

} // namespace common

#endif // COMMON_MAC_FILE_PROMISE_WRITER_H
