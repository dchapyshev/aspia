//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H
#define BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H

#include "build/build_config.h"

#if defined(OS_WIN)
#include <Windows.h>
#elif defined(OS_LINUX)
typedef union _XEvent XEvent;
#elif defined(OS_MAC)
#if defined(__OBJC__)
@class NSEvent;
#else  // __OBJC__
class NSEvent;
#endif // __OBJC__
#endif // OS_*

namespace base {

#if defined(OS_WIN)
using NativeEvent = MSG;
#elif defined(OS_LINUX)
using NativeEvent = XEvent*;
#elif defined(OS_MAC)
using NativeEvent = NSEvent*;
#else
#error Not implemented
#endif

// Dispatcher is used during a nested invocation of Run to dispatch events when
// |RunLoop(dispatcher).run()| is used.  If |RunLoop().run()| is invoked,
// MessageLoop does not dispatch events (or invoke TranslateMessage), rather
// every message is passed to Dispatcher's Dispatch method for dispatch. It is
// up to the Dispatcher whether or not to dispatch the event.
//
// The nested loop is exited by either posting a quit, or returning false
// from Dispatch.
class MessagePumpDispatcher
{
public:
    virtual ~MessagePumpDispatcher() = default;

    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool dispatch(const NativeEvent& event) = 0;
};

} // namespace base

#endif // BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H
