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

#include "base/linux/x_server_clipboard.h"

#include <QString>

#include <algorithm>
#include <iterator>
#include <mutex>

#include "base/logging.h"

namespace {

// Payloads larger than this are transferred incrementally (INCR), in chunks of this size.
constexpr size_t kIncrChunkSize = 256 * 1024;

// Upper bound for an incoming INCR transfer; a misbehaving owner cannot make us accumulate more.
constexpr size_t kMaxIncrDataSize = 16 * 1024 * 1024;

// An outgoing INCR transfer whose requestor has not consumed a chunk for this long is dropped.
constexpr std::chrono::seconds kOutgoingTransferTimeout(30);

// A capture session whose selection owner has not produced a reply or an INCR chunk for this long
// is finished with whatever it collected by onTick(): an owner that stops responding would
// otherwise hold the capture state (and the guard in onSetSelectionOwnerNotify()) forever. Kept
// generous so that an owner doing a slow lazy conversion of a large selection is not cut off.
constexpr std::chrono::seconds kCaptureStallTimeout(10);

// The clipboard displays whose X errors are ignored (the client runs one clipboard instance per
// session window, so there can be several), and the handler installed before ours.
std::mutex g_ignored_displays_lock;
std::set<Display*> g_ignored_displays;
XErrorHandler g_previous_error_handler = nullptr;
bool g_error_handler_installed = false;

//--------------------------------------------------------------------------------------------------
// Errors on the clipboard connections are expected: replies and INCR chunks target foreign
// requestor windows, which may be destroyed at any time (BadWindow), and the default Xlib handler
// would terminate the process. Errors of other connections are passed through. Installed once per
// process and never removed, so the reply paths stay fully asynchronous (no per-call XSync).
int xErrorHandler(Display* display, XErrorEvent* event)
{
    {
        std::scoped_lock lock(g_ignored_displays_lock);
        if (g_ignored_displays.count(display))
            return 0;
    }

    if (g_previous_error_handler)
        return g_previous_error_handler(display, event);

    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
XServerClipboard::XServerClipboard() = default;

//--------------------------------------------------------------------------------------------------
XServerClipboard::~XServerClipboard()
{
    std::scoped_lock lock(g_ignored_displays_lock);
    g_ignored_displays.erase(display_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::init(Display* display, const std::vector<std::string>& targets,
                            const ClipboardChangedCallback& callback)
{
    display_ = display;
    callback_ = callback;

    int xfixes_error_base;
    if (!XFixesQueryExtension(display_, &xfixes_event_base_, &xfixes_error_base))
    {
        LOG(ERROR) << "X server does not support XFixes";
        return;
    }

    // See xErrorHandler(): errors of the clipboard connection must not reach the fatal default
    // handler. Install only once: a repeated XSetErrorHandler() call on a later init() would
    // return our own handler and make it chain to itself forever.
    {
        std::scoped_lock lock(g_ignored_displays_lock);
        g_ignored_displays.insert(display_);

        if (!g_error_handler_installed)
        {
            g_previous_error_handler = XSetErrorHandler(xErrorHandler);
            g_error_handler_installed = true;
        }
    }

    clipboard_window_ = XCreateSimpleWindow(display_,
                                            DefaultRootWindow(display_),
                                            0, 0, 1, 1, // x, y, width, height
                                            0, 0, 0);

    // PropertyNotify events on our own window carry the chunks of incoming INCR transfers.
    XSelectInput(display_, clipboard_window_, PropertyChangeMask);

    clipboard_atom_ = XInternAtom(display_, "CLIPBOARD", X11_False);
    incr_atom_ = XInternAtom(display_, "INCR", X11_False);
    targets_atom_ = XInternAtom(display_, "TARGETS", X11_False);

    for (size_t i = 0; i < std::size(selection_data_atoms_); ++i)
    {
        const std::string name = "SELECTION_STRING_" + std::to_string(i);
        selection_data_atoms_[i] = XInternAtom(display_, name.c_str(), X11_False);
    }
    timestamp_atom_ = XInternAtom(display_, "TIMESTAMP", X11_False);
    utf8_string_atom_ = XInternAtom(display_, "UTF8_STRING", X11_False);

    for (const std::string& target : targets)
    {
        Atom atom = XInternAtom(display_, target.c_str(), X11_False);

        interesting_targets_.push_back(atom);
        target_names_[atom] = target;
        target_atoms_[target] = atom;
    }

    XFixesSelectSelectionInput(
        display_, clipboard_window_, clipboard_atom_, XFixesSetSelectionOwnerNotifyMask);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::setClipboard(FormatMap formats)
{
    DCHECK(display_);

    if (clipboard_window_ == BadValue)
        return;

    // The injected content replaces whatever a capture session was collecting: without the
    // cancellation the session would finish later (partially from our own fresh selection),
    // overwrite |data_| with the stale mix and send it back to the remote side. A pending
    // recapture is superseded for the same reason.
    cancelCapture();
    pending_capture_time_ = CurrentTime;

    data_.clear();
    for (auto& format : formats)
        data_[format.first] = std::make_shared<const std::string>(std::move(format.second));

    assertSelectionOwnership(XA_PRIMARY);
    assertSelectionOwnership(clipboard_atom_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::processXEvent(XEvent* event)
{
    if (clipboard_window_ == BadValue)
        return;

    switch (event->type)
    {
        case PropertyNotify:
            // Outgoing INCR transfers are driven by PropertyNotify events on requestor windows,
            // so this handler is not restricted to |clipboard_window_|.
            onPropertyNotify(event);
            break;
        case SelectionNotify:
            if (event->xany.window == clipboard_window_)
                onSelectionNotify(event);
            break;
        case SelectionRequest:
            if (event->xany.window == clipboard_window_)
                onSelectionRequest(event);
            break;
        case SelectionClear:
            if (event->xany.window == clipboard_window_)
                onSelectionClear(event);
            break;
        default:
            break;
    }

    if (event->type == xfixes_event_base_ + XFixesSetSelectionOwnerNotify)
    {
        XFixesSelectionNotifyEvent* notify_event =
            reinterpret_cast<XFixesSelectionNotifyEvent*>(event);
        onSetSelectionOwnerNotify(notify_event->selection, notify_event->timestamp);
    }
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onTick()
{
    if (clipboard_window_ == BadValue)
        return;

    // Finish a capture session whose owner has stopped responding with whatever it already
    // collected (an owner refusing one target must not cost the formats already retrieved), then
    // start the capture this session was blocking, if any.
    if (capture_progress_time_ != TimePoint() &&
        (Clock::now() - capture_progress_time_) >= kCaptureStallTimeout)
    {
        LOG(WARNING) << "Capture session made no progress for" << kCaptureStallTimeout.count()
                     << "seconds, finishing with what was captured";
        finishCapture(true);
    }

    // A transfer whose requestor died produces no PropertyNotify (its BadWindow errors are
    // swallowed) and no SelectionRequest may ever arrive to purge it; the entry would retain its
    // multi-megabyte payload for the process lifetime.
    purgeStaleTransfers();

    // Requests issued here (the pending recapture) are not flushed by the event pump, which only
    // runs when the socket is readable - and the server cannot reply to what it never received.
    XFlush(display_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSetSelectionOwnerNotify(Atom selection, Time timestamp)
{
    // Only process CLIPBOARD selections.
    if (selection != clipboard_atom_)
        return;

    // If we own the selection, don't request details for it.
    if (isSelectionOwner(selection))
        return;

    // Don't interrupt a running capture session: a copy made while the previous owner is still
    // being read is remembered and captured when the session ends instead of being dropped. A
    // session cannot block the pending copy for long - one that stops making progress is
    // cancelled by onTick().
    if (capture_progress_time_ != TimePoint())
    {
        pending_capture_time_ = timestamp;
        return;
    }

    startCapture(timestamp);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onPropertyNotify(XEvent* event)
{
    if (event->xproperty.window != clipboard_window_)
    {
        // A requestor has consumed a chunk of an outgoing INCR transfer by deleting the property.
        if (event->xproperty.state == PropertyDelete)
            continueOutgoingTransfer(event->xproperty.window, event->xproperty.atom);
        return;
    }

    // A chunk of an incoming INCR transfer.
    if (!incr_capture_active_ ||
        event->xproperty.atom != selectionDataProperty() ||
        event->xproperty.state != PropertyNewValue)
    {
        return;
    }

    Atom type;
    int format;
    unsigned long item_count;
    unsigned long after;
    unsigned char* data;

    XGetWindowProperty(display_, clipboard_window_, selectionDataProperty(),
                       0, ~0L, X11_True, AnyPropertyType, &type, &format,
                       &item_count, &after, &data);
    if (type == X11_None)
        return;

    if (item_count == 0)
    {
        // A zero-length chunk completes the transfer.
        capture_progress_time_ = Clock::now();

        if (!incr_capture_overflow_)
            captured_formats_[target_names_[current_target_]] = std::move(incr_capture_data_);

        incr_capture_active_ = false;
        incr_capture_data_.clear();

        XFree(data);
        finishCurrentTarget();
        return;
    }

    // Only a format-8 chunk that is still being accumulated counts as progress. A chunk of any
    // other format, or one drained after overflow (discarded anyway), must NOT refresh the stall
    // clock: otherwise an owner streaming such chunks forever would keep the session - and the
    // pending copy queued behind it - alive past kCaptureStallTimeout and wedge all further capture.
    if (format == 8 && !incr_capture_overflow_)
    {
        capture_progress_time_ = Clock::now();

        if (incr_capture_data_.size() + item_count <= kMaxIncrDataSize)
        {
            incr_capture_data_.append(reinterpret_cast<char*>(data), item_count);
        }
        else
        {
            LOG(WARNING) << "Incoming INCR transfer exceeds" << kMaxIncrDataSize
                         << "bytes, dropping format";
            incr_capture_overflow_ = true;
            incr_capture_data_.clear();
        }
    }

    XFree(data);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::onSelectionNotify(XEvent* event)
{
    XSelectionEvent* selection_event = &event->xselection;

    // Replies echo the timestamp of their request, but some hand-rolled owners reply with
    // CurrentTime instead, so timestamps cannot filter late replies of a cancelled session by
    // themselves. The reliable filter is the reply property: every request of a session (TARGETS
    // included) delivers into the session's data property, and the pool rotates when a session is
    // cancelled. A refusal carries no property to match and is accepted only with the exact
    // timestamp echo; a CurrentTime refusal stalls its target until onTick() cancels the session.
    if (selection_event->time != capture_time_ && selection_event->time != CurrentTime)
        return;

    if (selection_event->target == targets_atom_)
    {
        // Accept a TARGETS reply only while we are waiting for one: a late reply from a previous
        // slow owner must not restart the format queue of the current capture session.
        if (capture_progress_time_ == TimePoint() || current_target_ != X11_None)
            return;

        if (selection_event->property != selectionDataProperty() &&
            !(selection_event->property == X11_None && selection_event->time == capture_time_))
        {
            return;
        }

        capture_progress_time_ = Clock::now();
        handleTargetsNotify(selection_event);
        return;
    }

    // A reply for the format currently being retrieved. A late reply from a previous owner's slow
    // conversion may arrive after a new capture session has started, so the target must match the
    // one requested; otherwise the data would be stored under the wrong mime type and every
    // subsequent reply would shift by one queue slot.
    if (current_target_ == X11_None || selection_event->target != current_target_)
        return;

    if (selection_event->property == X11_None)
    {
        // The owner refused the conversion.
        if (selection_event->time != capture_time_)
            return;

        capture_progress_time_ = Clock::now();
        finishCurrentTarget();
        return;
    }

    // A reply of a cancelled session arrives with the previously used data property.
    if (selection_event->property != selectionDataProperty())
        return;

    capture_progress_time_ = Clock::now();

    Atom type;
    int format;
    unsigned long item_count;
    unsigned long after;
    unsigned char* data;

    XGetWindowProperty(display_, clipboard_window_, selection_event->property,
                       0, ~0L, X11_True, AnyPropertyType, &type, &format,
                       &item_count, &after, &data);
    if (type == incr_atom_)
    {
        // Large selection: the property carries only the lower bound of the size, and deleting it
        // (already done by XGetWindowProperty) tells the owner to start sending chunks via
        // PropertyNotify.
        incr_capture_active_ = true;
        incr_capture_overflow_ = false;
        incr_capture_data_.clear();

        if (data)
            XFree(data);
        return;
    }

    if (type != X11_None && format == 8 && data && item_count)
        captured_formats_[target_names_[current_target_]] = std::string(
            reinterpret_cast<char*>(data), item_count);

    if (data)
        XFree(data);

    finishCurrentTarget();
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
        else if (!sendDataResponse(selection_event.requestor, selection_event.property,
                                   selection_event.target))
        {
            selection_event.property = X11_None;
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
    std::vector<Atom> targets;
    targets.push_back(timestamp_atom_);
    targets.push_back(targets_atom_);

    for (const auto& format : data_)
    {
        auto it = target_atoms_.find(format.first);
        if (it != target_atoms_.end())
            targets.push_back(it->second);
    }

    // STRING is served with the UTF8_STRING payload.
    if (data_.count("UTF8_STRING") && !data_.count("STRING"))
        targets.push_back(XA_STRING);

    XChangeProperty(display_, requestor, property, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(targets.data()),
                    static_cast<int>(targets.size()));
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::sendTimestampResponse(Window requestor, Atom property)
{
    // Respond with the timestamp of our selection; we always return CurrentTime since our
    // selections are set by remote clients, so there is no associated local X event.
    Time time = CurrentTime;
    XChangeProperty(display_, requestor, property, XA_INTEGER, 32,
                    PropModeReplace, reinterpret_cast<unsigned char*>(&time), 1);
}

//--------------------------------------------------------------------------------------------------
bool XServerClipboard::sendDataResponse(Window requestor, Atom property, Atom target)
{
    auto name_it = target_names_.find(target);

    std::shared_ptr<const std::string> payload;

    auto it = data_.end();
    if (name_it != target_names_.end())
        it = data_.find(name_it->second);
    if (it != data_.end())
        payload = it->second;

    // STRING is served with the UTF8_STRING payload. ICCCM defines the STRING target as
    // ISO-8859-1 (still requested by xterm, xclip -t STRING and other non-UTF8-aware
    // applications), so the text is converted to Latin-1 when it is fully representable.
    // Otherwise (e.g. Cyrillic) the raw UTF-8 is passed through: a lossy '?' substitution would
    // destroy the text, while modern UTF-8 locales render the raw bytes correctly. The result is
    // cached in |data_| under the "STRING" key, so repeated requests do not re-convert.
    if (!payload && target == XA_STRING)
    {
        auto utf8_it = data_.find("UTF8_STRING");
        if (utf8_it != data_.end())
        {
            const QString text = QString::fromUtf8(
                utf8_it->second->data(), static_cast<qsizetype>(utf8_it->second->size()));

            const bool latin1_representable = std::all_of(text.begin(), text.end(),
                [](QChar c) { return c.unicode() <= 0xFF; });
            if (latin1_representable)
            {
                const QByteArray latin1 = text.toLatin1();
                payload = std::make_shared<const std::string>(latin1.constData(),
                                                              static_cast<size_t>(latin1.size()));
            }
            else
            {
                payload = utf8_it->second;
            }

            data_["STRING"] = payload;
        }
    }

    if (!payload || payload->empty())
        return false;

    const std::string& data = *payload;

    if (data.size() <= kIncrChunkSize)
    {
        XChangeProperty(display_, requestor, property, target, 8, PropModeReplace,
                        reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
                        static_cast<int>(data.size()));
        return true;
    }

    // The payload is too large for a single property: start an INCR transfer. The property holds
    // the total size, and the chunks are pushed as the requestor consumes (deletes) the property.
    // A retried request for the same requestor/property replaces the stalled transfer, otherwise
    // continueOutgoingTransfer() would keep feeding chunks from the old offset.
    auto existing = std::find_if(outgoing_transfers_.begin(), outgoing_transfers_.end(),
        [requestor, property](const OutgoingTransfer& transfer)
    {
        return transfer.requestor == requestor && transfer.property == property;
    });
    if (existing != outgoing_transfers_.end())
        outgoing_transfers_.erase(existing);

    XSelectInput(display_, requestor, PropertyChangeMask);

    long size = static_cast<long>(data.size());
    XChangeProperty(display_, requestor, property, incr_atom_, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&size), 1);

    OutgoingTransfer transfer;
    transfer.requestor = requestor;
    transfer.property = property;
    transfer.target = target;
    transfer.data = std::move(payload);
    transfer.start_time = Clock::now();
    outgoing_transfers_.push_back(std::move(transfer));

    return true;
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::continueOutgoingTransfer(Window requestor, Atom property)
{
    auto it = std::find_if(outgoing_transfers_.begin(), outgoing_transfers_.end(),
        [requestor, property](const OutgoingTransfer& transfer)
    {
        return transfer.requestor == requestor && transfer.property == property;
    });
    if (it == outgoing_transfers_.end())
        return;

    OutgoingTransfer& transfer = *it;
    transfer.start_time = Clock::now();

    if (transfer.offset < transfer.data->size())
    {
        const size_t chunk_size = std::min(kIncrChunkSize, transfer.data->size() - transfer.offset);

        XChangeProperty(display_, requestor, property, transfer.target, 8, PropModeReplace,
                        reinterpret_cast<unsigned char*>(
                            const_cast<char*>(transfer.data->data() + transfer.offset)),
                        static_cast<int>(chunk_size));
        transfer.offset += chunk_size;
        return;
    }

    // A zero-length chunk completes the transfer.
    XChangeProperty(display_, requestor, property, transfer.target, 8, PropModeReplace,
                    nullptr, 0);
    outgoing_transfers_.erase(it);

    unwatchRequestor(requestor);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::unwatchRequestor(Window requestor)
{
    // Keep watching while another transfer to the same window is still active.
    const bool has_other_transfers = std::any_of(
        outgoing_transfers_.begin(), outgoing_transfers_.end(),
        [requestor](const OutgoingTransfer& other) { return other.requestor == requestor; });
    if (has_other_transfers)
        return;

    XSelectInput(display_, requestor, NoEventMask);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::purgeStaleTransfers()
{
    const TimePoint now = Clock::now();

    auto it = outgoing_transfers_.begin();
    while (it != outgoing_transfers_.end())
    {
        if (now - it->start_time > kOutgoingTransferTimeout)
        {
            LOG(WARNING) << "Dropping stalled outgoing INCR transfer";

            const Window requestor = it->requestor;
            it = outgoing_transfers_.erase(it);
            unwatchRequestor(requestor);
        }
        else
        {
            ++it;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::startCapture(Time timestamp)
{
    cancelCapture();
    pending_capture_time_ = CurrentTime;
    capture_progress_time_ = Clock::now();

    // Requests are stamped with the session time (ICCCM discourages CurrentTime): replies of
    // well-behaved owners echo it.
    capture_time_ = timestamp;

    // Before getting the value of the chosen selection, request the list of target formats it
    // supports. The reply is delivered into the session data property, so onSelectionNotify()
    // can tell it apart from the late TARGETS reply of a cancelled session.
    XConvertSelection(display_, clipboard_atom_, targets_atom_, selectionDataProperty(),
                      clipboard_window_, capture_time_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::replayPendingCapture()
{
    if (pending_capture_time_ == CurrentTime)
        return;

    const Time timestamp = pending_capture_time_;
    pending_capture_time_ = CurrentTime;

    // Re-run the full notification checks: the pending owner may have been replaced by our own
    // ownership in the meantime.
    onSetSelectionOwnerNotify(clipboard_atom_, timestamp);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::handleTargetsNotify(XSelectionEvent* event)
{
    pending_targets_.clear();
    captured_formats_.clear();

    if (event->property != X11_None)
    {
        Atom type;
        int format;
        unsigned long item_count;
        unsigned long after;
        unsigned char* data;

        XGetWindowProperty(display_, clipboard_window_, event->property,
                           0, ~0L, X11_True, AnyPropertyType, &type, &format,
                           &item_count, &after, &data);
        if (type == incr_atom_)
        {
            // A TARGETS list large enough to require an INCR transfer is nonconforming; abandon the
            // session instead of letting the owner stream chunks into the session data property.
            if (data)
                XFree(data);
            finishCapture(true);
            return;
        }
        if (type != X11_None && format == 32 && data)
        {
            // The XGetWindowProperty man-page specifies that the returned property data will be an
            // array of |long|s in the case where |format| == 32.  Although the items are 32-bit
            // values (as stored and sent over the X protocol), Xlib presents the data to the client
            // as an array of |long|s, with zero-padding on a 64-bit system where |long| is bigger
            // than 32 bits.
            const long* targets = reinterpret_cast<const long*>(data);

            std::set<Atom> available;
            for (unsigned long i = 0; i < item_count; ++i)
                available.insert(static_cast<Atom>(targets[i]));

            for (Atom target : interesting_targets_)
            {
                if (!available.count(target))
                    continue;

                // STRING is captured only when UTF8_STRING is not offered.
                if (target == XA_STRING && available.count(utf8_string_atom_))
                    continue;

                pending_targets_.push_back(target);
            }
        }

        if (data)
            XFree(data);
    }

    // Owners commonly honor STRING conversions they do not advertise, so keep requesting the
    // plain text fallback when the target list is unusable or offers no text at all.
    const bool has_text = std::any_of(pending_targets_.begin(), pending_targets_.end(),
        [this](Atom target) { return target == utf8_string_atom_ || target == XA_STRING; });
    if (!has_text && target_names_.count(XA_STRING))
        pending_targets_.push_back(XA_STRING);

    requestNextTarget();
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::requestNextTarget()
{
    if (pending_targets_.empty())
    {
        finishCapture(false);
        return;
    }

    current_target_ = pending_targets_.front();
    pending_targets_.pop_front();

    XConvertSelection(display_, clipboard_atom_, current_target_, selectionDataProperty(),
                      clipboard_window_, capture_time_);
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::finishCurrentTarget()
{
    current_target_ = X11_None;
    incr_capture_active_ = false;

    requestNextTarget();
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::finishCapture(bool abandoned)
{
    if (!captured_formats_.empty())
    {
        // Refresh the served content too: we may still own the PRIMARY selection from an earlier
        // setClipboard() and would otherwise answer middle-click pastes with the stale payload.
        data_.clear();
        for (const auto& format : captured_formats_)
            data_[format.first] = std::make_shared<const std::string>(format.second);

        callback_(std::move(captured_formats_));
    }

    // An abandoned session left a conversion request outstanding; its owner may still reply late,
    // and a CurrentTime-stamped reply cannot be filtered by timestamp. Move to another data
    // property so that a late reply lands where nothing reads it. A completed session has no
    // outstanding request and keeps its property.
    if (abandoned)
        selection_data_index_ = (selection_data_index_ + 1) % std::size(selection_data_atoms_);

    resetCaptureState();
    replayPendingCapture();
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::cancelCapture()
{
    // Abandoning an active session leaves an outstanding request whose late reply must not land in
    // the property the next session uses (see finishCapture()); a session that already completed
    // and reset has none.
    if (capture_progress_time_ != TimePoint())
        selection_data_index_ = (selection_data_index_ + 1) % std::size(selection_data_atoms_);

    resetCaptureState();
}

//--------------------------------------------------------------------------------------------------
void XServerClipboard::resetCaptureState()
{
    capture_progress_time_ = TimePoint();
    capture_time_ = CurrentTime;
    pending_targets_.clear();
    current_target_ = X11_None;
    captured_formats_.clear();
    incr_capture_active_ = false;
    incr_capture_overflow_ = false;
    incr_capture_data_.clear();
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
        LOG(ERROR) << "XSetSelectionOwner failed for selection" << selection;
    }
}

//--------------------------------------------------------------------------------------------------
bool XServerClipboard::isSelectionOwner(Atom selection)
{
    return selections_owned_.find(selection) != selections_owned_.end();
}
