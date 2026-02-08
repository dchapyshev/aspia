//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_X11_X_SERVER_CLIPBOARD_H
#define BASE_X11_X_SERVER_CLIPBOARD_H

#include <QtGlobal>

#include <chrono>
#include <functional>
#include <set>
#include <string>

#include "base/x11/x11_headers.h"

namespace base {

class XServerClipboard
{
public:
    XServerClipboard();
    ~XServerClipboard();

    using ClipboardChangedCallback =
        std::function<void(const std::string& data)>;

    void init(Display* display, const ClipboardChangedCallback& callback);
    void setClipboard(const std::string& data);

    // Process |event| if it is an X selection notification. The caller should invoke this for every
    // event it receives from |display|.
    void processXEvent(XEvent* event);

private:
    // Handlers called by processXEvent() for each event type.
    void onSetSelectionOwnerNotify(Atom selection, Time timestamp);
    void onPropertyNotify(XEvent* event);
    void onSelectionNotify(XEvent* event);
    void onSelectionRequest(XEvent* event);
    void onSelectionClear(XEvent* event);

    // Used by onSelectionRequest() to respond to requests for details of our clipboard content.
    // This is done by changing the property |property| of the |requestor| window (these values come
    // from the XSelectionRequestEvent).
    // |target| must be a string type (STRING or UTF8_STRING).
    void sendTargetsResponse(Window requestor, Atom property);
    void sendTimestampResponse(Window requestor, Atom property);
    void sendStringResponse(Window requestor, Atom property, Atom target);

    // Called by onSelectionNotify() when the selection owner has replied to a request for
    // information about a selection.
    // |event| is the raw X event from the notification.
    // |type|, |format| etc are the results from XGetWindowProperty(), or 0 if there is no associated data.
    void handleSelectionNotify(XSelectionEvent* event,
                               Atom type,
                               int format,
                               int item_count,
                               void* data);

    // These methods return true if selection processing is complete, false otherwise. They are
    // called from handleSelectionNotify(), and take the same arguments.
    bool handleSelectionTargetsEvent(XSelectionEvent* event,
                                     int format,
                                     int item_count,
                                     void* data);
    bool handleSelectionStringEvent(XSelectionEvent* event,
                                    int format,
                                    int item_count,
                                    void* data);

    // Notify the registered callback of new clipboard text.
    void notifyClipboardText(const std::string& text);

    // These methods trigger the X server or selection owner to send back an event containing the
    // requested information.
    void requestSelectionTargets(Atom selection);
    void requestSelectionString(Atom selection, Atom target);

    // Assert ownership of the specified |selection|.
    void assertSelectionOwnership(Atom selection);
    bool isSelectionOwner(Atom selection);

    // Stores the Display* supplied to init().
    Display* display_ = nullptr;

    // Window through which clipboard events are received, or BadValue if the window could not be
    // created.
    Window clipboard_window_ = BadValue;

    // The event base returned by XFixesQueryExtension(). If XFixes is unavailable, the clipboard
    // window will not be created, and no event-processing will take place.
    int xfixes_event_base_ = -1;

    // Cached atoms for various strings, initialized during init().
    Atom clipboard_atom_ = X11_None;
    Atom large_selection_atom_ = X11_None;
    Atom selection_string_atom_ = X11_None;
    Atom targets_atom_ = X11_None;
    Atom timestamp_atom_ = X11_None;
    Atom utf8_string_atom_ = X11_None;

    // The set of X selections owned by |clipboard_window_| (can be Primary or Clipboard or both).
    std::set<Atom> selections_owned_;

    // Clipboard content to return to other applications when |clipboard_window_| owns a selection.
    std::string data_;

    // Stores the property to use for large transfers, or None if a large transfer is not currently
    // in-progress.
    Atom large_selection_property_ = X11_None;

    // Remembers the start time of selection processing, and is set to null when processing is
    // complete. This is used to decide whether to begin processing a new selection or continue with
    // the current selection.
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    TimePoint get_selections_time_;

    // |callback| argument supplied to init().
    ClipboardChangedCallback callback_;

    Q_DISABLE_COPY(XServerClipboard)
};

} // namespace base

#endif // BASE_X11_X_SERVER_CLIPBOARD_H
