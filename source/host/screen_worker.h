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

#ifndef HOST_SCREEN_WORKER_H
#define HOST_SCREEN_WORKER_H

#include "base/threading/worker.h"

// Runs the video pipeline of the desktop agent: screen capture, capture pacing and video/cursor
// encoding. On capture modes where the input injector is built on resources owned by the capturer
// (Wayland portal, VT terminals), the injector lives here too and InputWorker delegates to it.
class ScreenWorker final : public Worker
{
    Q_OBJECT

public:
    ScreenWorker();
    ~ScreenWorker() final;

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private:
    Q_DISABLE_COPY_MOVE(ScreenWorker)
};

#endif // HOST_SCREEN_WORKER_H
