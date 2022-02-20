//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_FILE_TRANSFER_WINDOW_H
#define CLIENT_FILE_TRANSFER_WINDOW_H

#include "client/file_transfer.h"

namespace client {

class FileTransferWindow
{
public:
    virtual ~FileTransferWindow() = default;

    virtual void start(std::shared_ptr<FileTransferProxy> transfer_proxy) = 0;
    virtual void stop() = 0;

    virtual void setCurrentItem(const std::string& source_path, const std::string& target_path) = 0;
    virtual void setCurrentProgress(int total, int current) = 0;
    virtual void errorOccurred(const FileTransfer::Error& error) = 0;
};

} // namespace client

#endif // CLIENT_FILE_TRANSFER_WINDOW_H
