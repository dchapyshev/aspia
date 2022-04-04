//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/x11/shared_x_display.h"

#include "base/logging.h"

#include <algorithm>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

namespace base {

SharedXDisplay::SharedXDisplay(Display* display)
    : display_(display)
{
    DCHECK(display_);
}

SharedXDisplay::~SharedXDisplay()
{
    DCHECK(event_handlers_.empty());
    XCloseDisplay(display_);
}

// static
base::local_shared_ptr<SharedXDisplay> SharedXDisplay::create(const std::string& display_name)
{
    Display* display = XOpenDisplay(display_name.empty() ? nullptr : display_name.c_str());
    if (!display)
    {
        LOG(LS_ERROR) << "Unable to open display";
        return nullptr;
    }

    return base::make_local_shared<SharedXDisplay>(display);
}

// static
base::local_shared_ptr<SharedXDisplay> SharedXDisplay::createDefault()
{
    return create(std::string());
}

void SharedXDisplay::addEventHandler(int type, XEventHandler* handler)
{
    event_handlers_[type].push_back(handler);
}

void SharedXDisplay::removeEventHandler(int type, XEventHandler* handler)
{
    EventHandlersMap::iterator handlers = event_handlers_.find(type);
    if (handlers == event_handlers_.end())
        return;

    std::vector<XEventHandler*>::iterator new_end =
        std::remove(handlers->second.begin(), handlers->second.end(), handler);
    handlers->second.erase(new_end, handlers->second.end());

    // Check if no handlers left for this event.
    if (handlers->second.empty())
        event_handlers_.erase(handlers);
}

void SharedXDisplay::processPendingXEvents()
{
    // Hold reference to |this| to prevent it from being destroyed while processing events.
    base::local_shared_ptr<SharedXDisplay> self = shared_from_this();

    // Find the number of events that are outstanding "now."  We don't just loop on XPending because
    // we want to guarantee this terminates.
    int events_to_process = XPending(display());
    XEvent e;

    for (int i = 0; i < events_to_process; i++)
    {
        XNextEvent(display(), &e);

        EventHandlersMap::iterator handlers = event_handlers_.find(e.type);
        if (handlers == event_handlers_.end())
            continue;

        for (std::vector<XEventHandler*>::iterator it = handlers->second.begin();
             it != handlers->second.end(); ++it)
        {
            if ((*it)->handleXEvent(e))
                break;
        }
    }
}

void SharedXDisplay::ignoreXServerGrabs()
{
    int test_event_base = 0;
    int test_error_base = 0;
    int major = 0;
    int minor = 0;

    if (XTestQueryExtension(display(), &test_event_base, &test_error_base, &major, &minor))
    {
        XTestGrabControl(display(), true);
    }
}

} // namespace base
