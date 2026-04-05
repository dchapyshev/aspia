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

#include "common/mac/file_promise_writer.h"

#include "base/logging.h"

namespace common {

//--------------------------------------------------------------------------------------------------
FilePromiseWriter::FilePromiseWriter(qint64 file_size)
    : file_size_(file_size)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FilePromiseWriter::~FilePromiseWriter()
{
    terminate();
}

//--------------------------------------------------------------------------------------------------
void FilePromiseWriter::addData(const QByteArray& data, bool is_last)
{
    {
        std::scoped_lock lock(mutex_);

        if (!data.isEmpty())
            buffer_.append(data);

        if (is_last)
            is_finished_ = true;
    }

    cv_.notify_one();
}

//--------------------------------------------------------------------------------------------------
void FilePromiseWriter::terminate()
{
    {
        std::scoped_lock lock(mutex_);
        is_terminated_ = true;
    }

    cv_.notify_one();
}

//--------------------------------------------------------------------------------------------------
bool FilePromiseWriter::writeToDestination(const QString& dest_path)
{
    QFile file(dest_path);

    if (!file.open(QFile::WriteOnly))
    {
        LOG(ERROR) << "Failed to open destination file:" << dest_path;
        return false;
    }

    for (;;)
    {
        QByteArray chunk;
        bool finished = false;
        bool terminated = false;

        {
            std::unique_lock lock(mutex_);

            cv_.wait(lock, [this]()
            {
                return !buffer_.isEmpty() || is_finished_ || is_terminated_;
            });

            terminated = is_terminated_;
            if (terminated)
            {
                LOG(INFO) << "Transfer terminated for:" << dest_path;
                break;
            }

            chunk = std::move(buffer_);
            buffer_.clear();
            finished = is_finished_;
        }

        if (!chunk.isEmpty())
        {
            qint64 written = file.write(chunk);
            if (written != chunk.size())
            {
                LOG(ERROR) << "Write error for:" << dest_path
                           << "expected:" << chunk.size()
                           << "written:" << written;
                file.close();
                return false;
            }
        }

        if (finished)
            break;
    }

    file.close();
    return !is_terminated_;
}

} // namespace common
