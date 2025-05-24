//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_FILE_AGENT_H
#define HOST_FILE_AGENT_H

#include "base/macros_magic.h"
#include "base/ipc/ipc_channel.h"
#include "common/file_worker.h"

namespace host {

class FileAgent final : public QObject
{
    Q_OBJECT

public:
    explicit FileAgent(QObject* parent = nullptr);
    ~FileAgent();

    void start(const QString& channel_id);

private slots:
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);

private:
    base::IpcChannel* channel_ = nullptr;
    common::FileWorker worker_;

    proto::FileRequest request_;
    proto::FileReply reply_;

    DISALLOW_COPY_AND_ASSIGN(FileAgent);
};

} // namespace host

#endif // HOST_FILE_AGENT_H
