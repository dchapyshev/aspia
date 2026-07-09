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

#ifndef HOST_IPC_WORKER_H
#define HOST_IPC_WORKER_H

#include "base/threading/worker.h"

// Runs the I/O side of the desktop agent: the IPC connection to the service and the per-client
// protocol state. Incoming messages are routed directly to the other workers; outgoing media
// packets produced by them are sent to the clients from here.
class IpcWorker final : public Worker
{
    Q_OBJECT

public:
    IpcWorker();
    ~IpcWorker() final;

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    Q_DISABLE_COPY_MOVE(IpcWorker)
};

#endif // HOST_IPC_WORKER_H
