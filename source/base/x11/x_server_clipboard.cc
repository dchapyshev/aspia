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

#include "base/x11/x_server_clipboard.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
XServerClipboard::XServerClipboard() = default;

//--------------------------------------------------------------------------------------------------
XServerClipboard::~XServerClipboard() = default;

//--------------------------------------------------------------------------------------------------
void XServerClipboard::init(Display* display, const ClipboardChangedCallback& callback)
{
    display_ = display;
    callback_ = callback;

    // If any of these X API calls fail, an X Error will be raised, crashing the process. This is
    // unlikely to occur in practice, and even if it does, it would mean the X server is in a bad
    // state, so it's not worth trying to trap such errors here.

    // TODO(lambroslambrou): Consider using ScopedXErrorHandler here, or consider placing
    // responsibility for handling X Errors outside this class, since X Error handlers are global
    // to all X connections.
    int xfixes_error_base;
    if (!XFixesQueryExtension(display_, &xfixes_event_base_, &xfixes_error_base))
    {
        LOG(ERROR) << "X server does not support XFixes";
        return;
    }

    clipboard_window_ = XCreateSimpleWindow(display_,
                                            DefaultRootWindow(display_),
                                            0, 0, 1, 1,  // x, y, width, height
                                            0, 0, 0);

    static const char* const kAtomNames[] =
    {
        "CLIPBOARD",
        "INCR",
        "SELECTION_STRING",
        "TARGETS",
        "TIMESTAMP",
        "UTF8_STRING"
    };
    static const int kNumAtomNames = std::size(kAtomNames);

    Atom atoms[kNumAtomNames];
    if (XInternAtoms(display_, const_cast<char**>(kAtomNames), kNumAtomNames, X11_False, atoms))
    {
        clipboard_atom_ = atoms[0];
        large_selection_atom_ = atoms[1];
        selection_string_atom_ = atoms[2];
        targets_atom_ = atoms[3];
        timestamp_atom_ = atoms[4];
        utf8_string_atom_ = atoms[5];
        static_assert(kNumAtomNames >= 6, "kAtomNames is too small");
    }
    else
    {
        LOG(ERROR) << "XInternAtoms failed";
    }

    XFixesSelectSelectionInput(display_, clipboard_window_, clipboard_atom_,
                               XFixesSetSelectionOwnerNotifyMask);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::setClipboard(const std::string& data)
{
    DCHECK(display_);

    if (clipboard_window_ == BadValue)
      return;

    data_ = data;

    assertSelectionOwnership(XA_PRIMARY);
    assertSelectionOwnership(clipboard_atom_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::processXEvent(XEvent* event)
{
    if (clipboard_window_ == BadValue || event->xany.window != clipboard_window_)
        return;

    switch (event->type)
    {
        case PropertyNotify:
            onPropertyNotify(event);
            break;
        case SelectionNotify:
            onSelectionNotify(event);
            break;
        case SelectionRequest:
            onSelectionRequest(event);
            break;
        case SelectionClear:
            onSelectionClear(event);
            break;
        default:
            break;
    }

    if (event->type == xfixes_event_base_ + XFixesSetSelectionOwnerNotify)
    {
        XFixesSelectionNotifyEvent* notify_event =
            reinterpret_cast<XFixesSelectionNotifyEvent*>(event);
        onSetSelectionOwnerNotify(notify_event->selection, notify_event->selection_timestamp);
    }
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSetSelectionOwnerNotify(Atom selection, Time /* timestamp */)
{
    // Protect against receiving new XFixes selection notifications whilst we're in the middle of
    // waiting for information from the current selection owner. A reasonable timeout allows for
    // misbehaving apps that don't respond quickly to our requests.
    if (get_selections_time_ != TimePoint() &&
        (Clock::now() - get_selections_time_) < std::chrono::seconds(5))
    {
        // TODO(lambroslambrou): Instead of ignoring this notification, cancel any pending request
        // operations and ignore the resulting events, before dispatching new requests here.
        return;
    }

    // Only process CLIPBOARD selections.
    if (selection != clipboard_atom_)
        return;

    // If we own the selection, don't request details for it.
    if (isSelectionOwner(selection))
        return;

    get_selections_time_ = Clock::now();

    // Before getting the value of the chosen selection, request the list of target formats it
    // supports.
    requestSelectionTargets(selection);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onPropertyNotify(XEvent* event)
{
    if (large_selection_property_ != X11_None &&
        event->xproperty.atom == large_selection_property_ &&
        event->xproperty.state == PropertyNewValue)
    {
        Atom type;
        int format;
        unsigned long item_count, after;
        unsigned char *data;

        XGetWindowProperty(display_, clipboard_window_, large_selection_property_,
                         0, ~0L, X11_True, AnyPropertyType, &type, &format,
                         &item_count, &after, &data);
        if (type != X11_None)
        {
            // TODO(lambroslambrou): Properly support large transfers - http://crbug.com/151447.
            XFree(data);

            // If the property is zero-length then the large transfer is complete.
            if (item_count == 0)
                large_selection_property_ = X11_None;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSelectionNotify(XEvent* event)
{
    if (event->xselection.property != X11_None)
    {
        Atom type;
        int format;
        unsigned long item_count, after;
        unsigned char *data;

        XGetWindowProperty(display_, clipboard_window_, event->xselection.property,
                           0, ~0L, X11_True, AnyPropertyType, &type, &format,
                           &item_count, &after, &data);
        if (type == large_selection_atom_)
        {
            // Large selection - just read and ignore these for now.
            large_selection_property_ = event->xselection.property;
        }
        else
        {
            // Standard selection - call the selection notifier.
            large_selection_property_ = X11_None;
            if (type != X11_None)
            {
                handleSelectionNotify(&event->xselection, type, format, item_count, data);
                XFree(data);
                return;
            }
        }
    }
    handleSelectionNotify(&event->xselection, 0, 0, 0, 0);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSelectionRequest(XEvent* event)
{
    XSelectionEvent selection_event;
    selection_event.type = SelectionNotify;
    selection_event.display = event->xselectionrequest.display;
    selection_event.requestor = event->xselectionrequest.requestor;
    selection_event.selection = event->xselectionrequest.selection;
    selection_event.time = event->xselectionrequest.time;
    selection_event.target = event->xselectionrequest.target;

    if (event->xselectionrequest.property == X11_None)
         event->xselectionrequest.property = event->xselectionrequest.target;

    if (!isSelectionOwner(selection_event.selection))
    {
        selection_event.property = X11_None;
    }
    else
    {
        selection_event.property = event->xselectionrequest.property;
        if (selection_event.target == targets_atom_)
        {
            sendTargetsResponse(selection_event.requestor, selection_event.property);
        }
        else if (selection_event.target == timestamp_atom_)
        {
            sendTimestampResponse(selection_event.requestor, selection_event.property);
        }
        else if (selection_event.target == utf8_string_atom_ || selection_event.target == XA_STRING)
        {
            sendStringResponse(selection_event.requestor, selection_event.property,
                               selection_event.target);
        }
    }
    XSendEvent(display_, selection_event.requestor, X11_False, 0,
               reinterpret_cast<XEvent*>(&selection_event));
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSelectionClear(XEvent* event)
{
    selections_owned_.erase(event->xselectionclear.selection);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::sendTargetsResponse(Window requestor, Atom property)
{
    // Respond advertising XA_STRING, UTF8_STRING and TIMESTAMP data for the selection.
    Atom targets[3];
    targets[0] = timestamp_atom_;
    targets[1] = utf8_string_atom_;
    targets[2] = XA_STRING;
    XChangeProperty(display_, requestor, property, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(targets), 3);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::sendTimestampResponse(Window requestor, Atom property)
{
    // Respond with the timestamp of our selection; we always return CurrentTime since our
    // selections are set by remote clients, so there is no associated local X event.

    // TODO(lambroslambrou): Should use a proper timestamp here instead of CurrentTime. ICCCM
    // recommends doing a zero-length property append, and getting a timestamp from the subsequent
    // PropertyNotify event.
    Time time = CurrentTime;
    XChangeProperty(display_, requestor, property, XA_INTEGER, 32,
                    PropModeReplace, reinterpret_cast<unsigned char*>(&time), 1);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::sendStringResponse(Window requestor, Atom property, Atom target)
{
    if (!data_.empty())
    {
        // Return the actual string data; we always return UTF8, regardless of the configured locale.
        XChangeProperty(display_, requestor, property, target, 8, PropModeReplace,
                        reinterpret_cast<unsigned char*>(const_cast<char*>(data_.data())),
                        data_.size());
    }
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::handleSelectionNotify(XSelectionEvent* event,
                                             Atom /* type */,
                                             int format,
                                             int item_count,
                                             void* data)
{
    bool finished = false;

    if (event->target == targets_atom_)
    {
        finished = handleSelectionTargetsEvent(event, format, item_count, data);
    }
    else if (event->target == utf8_string_atom_ || event->target == XA_STRING)
    {
        finished = handleSelectionStringEvent(event, format, item_count, data);
    }

    if (finished)
        get_selections_time_ = TimePoint();
}

//--------------------------------------------------------------------------------------------------
bool XServerClipboard::handleSelectionTargetsEvent(XSelectionEvent* event,
                                                   int format,
                                                   int item_count,
                                                   void* data)
{
    if (event->property == targets_atom_)
    {
        if (data && format == 32)
        {
            // The XGetWindowProperty man-page specifies that the returned property data will be an
            // array of |long|s in the case where |format| == 32.  Although the items are 32-bit
            // values (as stored and sent over the X protocol), Xlib presents the data to the client
            // as an array of |long|s, with zero-padding on a 64-bit system where |long| is bigger
            // than 32 bits.
            const long* targets = static_cast<const long*>(data);
            for (int i = 0; i < item_count; i++)
            {
                if (targets[i] == static_cast<long>(utf8_string_atom_))
                {
                    requestSelectionString(event->selection, utf8_string_atom_);
                    return false;
                }
            }
        }
    }
    requestSelectionString(event->selection, XA_STRING);
    return false;
}

//--------------------------------------------------------------------------------------------------
bool XServerClipboard::handleSelectionStringEvent(XSelectionEvent* event,
                                                  int format,
                                                  int item_count,
                                                  void* data)
{
    if (event->property != selection_string_atom_ || !data || format != 8)
        return true;

    if (event->target == XA_STRING || event->target == utf8_string_atom_)
    {
        std::string text(static_cast<char*>(data), item_count);
        notifyClipboardText(text);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::notifyClipboardText(const std::string& text)
{
    data_ = text;
    callback_(data_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::requestSelectionTargets(Atom selection)
{
    XConvertSelection(display_, selection, targets_atom_, targets_atom_,
                      clipboard_window_, CurrentTime);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::requestSelectionString(Atom selection, Atom target)
{
    XConvertSelection(display_, selection, target, selection_string_atom_,
                      clipboard_window_, CurrentTime);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::assertSelectionOwnership(Atom selection)
{
    XSetSelectionOwner(display_, selection, clipboard_window_, CurrentTime);
    if (XGetSelectionOwner(display_, selection) == clipboard_window_)
    {
        selections_owned_.insert(selection);
    }
    else
    {
        LOG(ERROR) << "XSetSelectionOwner failed for selection " << selection;
    }
}

//--------------------------------------------------------------------------------------------------
bool XServerClipboard::isSelectionOwner(Atom selection)
{
    return selections_owned_.find(selection) != selections_owned_.end();
}

} // namespace base
