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

#include "host/audio_worker.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
// On macOS the Qt dispatcher backs the thread with a CFRunLoop (via
// QT_EVENT_DISPATCHER_CORE_FOUNDATION); everywhere else asio is used.
AudioWorker::AudioWorker()
#if defined(Q_OS_MACOS)
    : Worker(Thread::QtDispatcher)
#else
    : Worker(Thread::AsioDispatcher)
#endif
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AudioWorker::~AudioWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStart()
{
    LOG(INFO) << "Audio worker started";
}

//--------------------------------------------------------------------------------------------------
void AudioWorker::onStop()
{
    LOG(INFO) << "Audio worker stopped";
}
