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

#ifndef HOST_WORKERS_SYS_INFO_WORKER_H
#define HOST_WORKERS_SYS_INFO_WORKER_H

#include <QByteArray>

#include "base/threading/worker.h"

class SysInfoWorker final : public Worker
{
    Q_OBJECT

public:
    SysInfoWorker();
    ~SysInfoWorker() final;

    static quint32 createConsumerId();

public slots:
    // Parses the serialized SystemInfoRequest in |buffer|, builds the report and announces it with
    // sig_systemInfo. Connect to it with Qt::QueuedConnection: building the report takes seconds and
    // has to happen in the thread of the worker.
    void onQuery(quint32 consumer_id, const QByteArray& buffer);

signals:
    // Emitted from the worker thread with the serialized SystemInfo built for an onQuery() call.
    // |consumer_id| is the one that came with that call.
    void sig_systemInfo(quint32 consumer_id, const QByteArray& buffer);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    Q_DISABLE_COPY_MOVE(SysInfoWorker)
};

#endif // HOST_WORKERS_SYS_INFO_WORKER_H
