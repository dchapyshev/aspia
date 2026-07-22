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

#include <functional>

#include "base/threading/worker.h"

class SysInfoWorker final : public Worker
{
    Q_OBJECT

public:
    SysInfoWorker();
    ~SysInfoWorker() final;

    // Parses the serialized SystemInfoRequest in |buffer|, builds the report in the worker thread
    // and delivers the serialized reply (empty on a malformed request) to |reply| in the calling
    // worker's thread. The reply is dropped if |context| is destroyed before the report is ready.
    // May be called from any worker thread.
    void query(QObject* context, const QByteArray& buffer, std::function<void(QByteArray)> reply);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    Q_DISABLE_COPY_MOVE(SysInfoWorker)
};

#endif // HOST_WORKERS_SYS_INFO_WORKER_H
