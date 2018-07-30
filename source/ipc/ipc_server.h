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

#ifndef ASPIA_IPC__IPC_SERVER_H_
#define ASPIA_IPC__IPC_SERVER_H_

#include <QPointer>

#include "base/macros_magic.h"

class QLocalServer;

namespace aspia {

class IpcChannel;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer() = default;

    bool isStarted() const;

public slots:
    void start();
    void stop();

signals:
    void started(const QString& channel_id);
    void finished();
    void newConnection(IpcChannel* channel);
    void errorOccurred();

private slots:
    void onNewConnection();

private:
    QPointer<QLocalServer> server_;

    DISALLOW_COPY_AND_ASSIGN(IpcServer);
};

} // namespace aspia

#endif // ASPIA_IPC__IPC_SERVER_H_
