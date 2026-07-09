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

#include "base/threading/thread.h"

#include "base/logging.h"
#include "base/threading/asio_event_dispatcher.h"

#if defined(Q_OS_WINDOWS)
#include <optional>
#include "base/win/scoped_com_initializer.h"
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
Thread::Thread(EventDispatcher dispatcher, QObject* parent)
    : QThread(parent),
      dispatcher_(dispatcher)
{
    if (dispatcher == AsioDispatcher)
        setEventDispatcher(new AsioEventDispatcher());
}

//--------------------------------------------------------------------------------------------------
Thread::~Thread()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
void Thread::stop()
{
    quit();
    wait();
}

//--------------------------------------------------------------------------------------------------
void Thread::run()
{
#if defined(Q_OS_WINDOWS)
    // Asio threads join the COM multithreaded apartment. The default STA creates a hidden OLE
    // message window on the thread.
    std::optional<ScopedCOMInitializer> com_initializer;
    if (dispatcher_ == AsioDispatcher)
        com_initializer.emplace(ScopedCOMInitializer::kMTA);
    else
        com_initializer.emplace();
    CHECK(com_initializer->isSucceeded());
#endif // defined(Q_OS_WINDOWS)

    emit sig_beforeRunning();
    exec();
    emit sig_afterRunning();
}
