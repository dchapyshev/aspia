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

#ifndef BASE_DESKTOP_X11_SHARED_X_DISPLAY_H
#define BASE_DESKTOP_X11_SHARED_X_DISPLAY_H

#include <QtGlobal>

#include <map>
#include <memory>
#include <string>
#include <vector>

// Including Xlib.h will involve evil defines (Bool, Status, True, False), which
// easily conflict with other headers.
typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

namespace base {

// A ref-counted object to store XDisplay connection.
class SharedXDisplay : public std::enable_shared_from_this<SharedXDisplay>
{
public:
    class XEventHandler
    {
        public:
        virtual ~XEventHandler() = default;

        // Processes XEvent. Returns true if the event has been handled.
        virtual bool handleXEvent(const XEvent& event) = 0;
    };

    // Takes ownership of |display|.
    explicit SharedXDisplay(Display* display);
    ~SharedXDisplay();

    // Creates a new X11 Display for the |display_name|. NULL is returned if X11 connection failed.
    // Equivalent to CreateDefault() when |display_name| is empty.
    static std::shared_ptr<SharedXDisplay> create(const QString& display_name);

    // Creates X11 Display connection for the default display (e.g. specified in DISPLAY). NULL is
    // returned if X11 connection failed.
    static std::shared_ptr<SharedXDisplay> createDefault();

    Display* display() { return display_; }

    // Adds a new event |handler| for XEvent's of |type|.
    void addEventHandler(int type, XEventHandler* handler);

    // Removes event |handler| added using |AddEventHandler|. Doesn't do anything if |handler| is
    // not registered.
    void removeEventHandler(int type, XEventHandler* handler);

    // Processes pending XEvents, calling corresponding event handlers.
    void processPendingXEvents();

    void ignoreXServerGrabs();

private:
    typedef std::map<int, std::vector<XEventHandler*> > EventHandlersMap;

    Display* display_;
    EventHandlersMap event_handlers_;

    Q_DISABLE_COPY(SharedXDisplay)
};

} // namespace base

#endif // BASE_DESKTOP_X11_SHARED_X_DISPLAY_H
