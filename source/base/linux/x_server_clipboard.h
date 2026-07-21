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

#ifndef BASE_LINUX_X_SERVER_CLIPBOARD_H
#define BASE_LINUX_X_SERVER_CLIPBOARD_H

#include <QtClassHelperMacros>

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/linux/x11_headers.h"

class XServerClipboard
{
public:
    XServerClipboard();
    ~XServerClipboard();

    // Clipboard content as a set of representations keyed by X target name (e.g. "UTF8_STRING",
    // "text/html", "image/png").
    using FormatMap = std::map<std::string, std::string>;

    using ClipboardChangedCallback = std::function<void(FormatMap&& formats)>;

    // |targets| lists the X target names to capture from other applications and to serve to them.
    // "STRING" is captured only when "UTF8_STRING" is not offered, and is served with the
    // "UTF8_STRING" payload.
    void init(Display* display, const std::vector<std::string>& targets,
              const ClipboardChangedCallback& callback);

    void setClipboard(FormatMap formats);

    // Process |event| if it is an X selection notification. The caller should invoke this for every
    // event it receives from |display|.
    void processXEvent(XEvent* event);

    // Periodic maintenance driven by the owner's clock (about one call per second): cancels a
    // capture session whose owner stopped responding and drops outgoing transfers whose requestor
    // died. Event-driven recovery is not enough - the dedicated clipboard connection may carry no
    // traffic at all while a session is stuck.
    void onTick();

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
    void sendTargetsResponse(Window requestor, Atom property);
    void sendTimestampResponse(Window requestor, Atom property);

    // Responds with the data for |target|, starting an incremental (INCR) transfer when the payload
    // exceeds the chunk size. Returns false if there is no data for |target|.
    bool sendDataResponse(Window requestor, Atom property, Atom target);

    // Continues the outgoing INCR transfer for |requestor|/|property| after the requestor has
    // consumed (deleted) the previous chunk.
    void continueOutgoingTransfer(Window requestor, Atom property);

    // Stops watching |requestor| unless another outgoing transfer to the same window is active.
    void unwatchRequestor(Window requestor);

    // Drops outgoing transfers whose requestor has stopped consuming chunks.
    void purgeStaleTransfers();

    // Capture side: TARGETS reply handling and sequential retrieval of the offered formats.
    void startCapture(Time timestamp);
    void replayPendingCapture();
    void handleTargetsNotify(XSelectionEvent* event);
    void requestNextTarget();
    void finishCurrentTarget();
    void finishCapture(bool abandoned);
    void cancelCapture();
    void resetCaptureState();

    // The property the current capture session receives selection data into.
    Atom selectionDataProperty() const { return selection_data_atoms_[selection_data_index_]; }

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
    Atom incr_atom_ = X11_None;
    Atom targets_atom_ = X11_None;
    Atom timestamp_atom_ = X11_None;
    Atom utf8_string_atom_ = X11_None;

    // Properties for receiving selection data. Capture sessions rotate over the pool, so that
    // INCR chunks from a stalled previous owner keep landing in an old property and cannot mix
    // into the current session (PropertyNotify carries no sender identity).
    Atom selection_data_atoms_[4] = { X11_None, X11_None, X11_None, X11_None };
    size_t selection_data_index_ = 0;

    // X server time of the capture session start, used to stamp the session's requests. Late
    // replies of a cancelled session are filtered by the session data property; the timestamp
    // echo is required only for refusals, which carry no property to match.
    Time capture_time_ = CurrentTime;

    // Ownership-change timestamp of a copy made while a capture session was still running, to be
    // captured when the session ends. CurrentTime when there is no pending copy.
    Time pending_capture_time_ = CurrentTime;

    // Targets supplied to init(), in the preferred capture order, and the atom <-> name mapping.
    std::vector<Atom> interesting_targets_;
    std::map<Atom, std::string> target_names_;
    std::map<std::string, Atom> target_atoms_;

    // The set of X selections owned by |clipboard_window_| (can be Primary or Clipboard or both).
    std::set<Atom> selections_owned_;

    // Clipboard content to return to other applications when |clipboard_window_| owns a selection.
    std::map<std::string, std::shared_ptr<const std::string>> data_;

    // Capture state: the queue of targets left to retrieve from the current selection owner, the
    // target being retrieved and the representations collected so far.
    std::deque<Atom> pending_targets_;
    Atom current_target_ = X11_None;
    FormatMap captured_formats_;

    // Incoming INCR transfer state for |current_target_|.
    bool incr_capture_active_ = false;
    bool incr_capture_overflow_ = false;
    std::string incr_capture_data_;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    // An outgoing INCR transfer: a requestor consumes |data| in chunks by deleting |property|
    // after reading each part.
    struct OutgoingTransfer
    {
        Window requestor;
        Atom property;
        Atom target;
        std::shared_ptr<const std::string> data;
        size_t offset = 0;
        TimePoint start_time;
    };

    std::vector<OutgoingTransfer> outgoing_transfers_;

    // Time of the last reply or INCR chunk accepted by the capture session, null when no session
    // is active. A session whose owner stopped responding is cancelled by onTick() after
    // kCaptureStallTimeout without progress.
    TimePoint capture_progress_time_;

    // |callback| argument supplied to init().
    ClipboardChangedCallback callback_;

    Q_DISABLE_COPY(XServerClipboard)
};

#endif // BASE_LINUX_X_SERVER_CLIPBOARD_H
