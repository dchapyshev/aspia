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

#ifndef HOST_INPUT_WORKER_H
#define HOST_INPUT_WORKER_H

#include "base/threading/worker.h"

// Injects the input received from clients into the user session. Runs on its own thread so a busy
// video pipeline never delays keystrokes and pointer motion. Where the platform ties the injector
// to capturer-owned resources (Wayland portal, VT terminals), injection is delegated to ScreenWorker.
class InputWorker final : public Worker
{
    Q_OBJECT

public:
    InputWorker();
    ~InputWorker() final;

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    Q_DISABLE_COPY_MOVE(InputWorker)
};

#endif // HOST_INPUT_WORKER_H
