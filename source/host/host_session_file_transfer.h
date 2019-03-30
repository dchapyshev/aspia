//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__HOST_SESSION_FILE_TRANSFER_H
#define HOST__HOST_SESSION_FILE_TRANSFER_H

#include "host/host_session.h"

namespace common {
class FileWorker;
} // namespace common

namespace host {

class SessionFileTransfer : public Session
{
    Q_OBJECT

public:
    explicit SessionFileTransfer(const QString& channel_id);
    ~SessionFileTransfer() = default;

protected:
    // Session implementation.
    void sessionStarted() override;
    void messageReceived(const QByteArray& buffer) override;

private:
    common::FileWorker* worker_;

    DISALLOW_COPY_AND_ASSIGN(SessionFileTransfer);
};

} // namespace host

#endif // HOST__HOST_SESSION_FILE_TRANSFER_H
