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

#ifndef HOST_FILE_AGENT_H
#define HOST_FILE_AGENT_H

#include <QObject>

#include "base/serialization.h"
#include "base/desktop/power_save_blocker.h"
#include "proto/file_transfer.h"

class FileWorker;
class IpcChannel;

class FileAgent final : public QObject
{
    Q_OBJECT

public:
    explicit FileAgent(QObject* parent = nullptr);
    ~FileAgent();

    void start(const QString& channel_id);

private slots:
    void onIpcConnected();
    void onIpcDisconnected();
    void onIpcErrorOccurred();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable);

private:
    IpcChannel* ipc_channel_ = nullptr;
    FileWorker* worker_ = nullptr;

    PowerSaveBlocker power_save_blocker_;

    Parser<proto::file_transfer::Request> request_;
    Serializer<proto::file_transfer::Reply> reply_;

    Q_DISABLE_COPY_MOVE(FileAgent)
};

#endif // HOST_FILE_AGENT_H
