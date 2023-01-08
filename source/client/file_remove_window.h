//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_FILE_REMOVE_WINDOW_H
#define CLIENT_FILE_REMOVE_WINDOW_H

#include "proto/file_transfer.pb.h"

namespace client {

class FileRemoverProxy;

class FileRemoveWindow
{
public:
    virtual ~FileRemoveWindow() = default;

    virtual void start(std::shared_ptr<FileRemoverProxy> remover_proxy) = 0;
    virtual void stop() = 0;

    virtual void setCurrentProgress(const std::string& name, int percentage) = 0;
    virtual void errorOccurred(const std::string& path,
                               proto::FileError error_code,
                               uint32_t available_actions) = 0;
};

} // namespace client

#endif // CLIENT_FILE_REMOVE_WINDOW_H
