//
// PROJECT:         Aspia
// FILE:            protocol/clipboard.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CLIPBOARD_H
#define _ASPIA_PROTOCOL__CLIPBOARD_H

#include <memory>

#include "base/message_window.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class Clipboard
{
public:
    Clipboard() = default;
    ~Clipboard();

    using ClipboardEventCallback = std::function<void(proto::desktop::ClipboardEvent& event)>;

    // Callback is called when there is an outgoing clipboard.
    bool Start(ClipboardEventCallback clipboard_event_callback);

    void Stop();

    // Receiving the incoming clipboard.
    void InjectClipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    // Handles messages received by |window_|.
    bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    void OnClipboardUpdate();

    // Used to subscribe to WM_CLIPBOARDUPDATE messages.
    std::unique_ptr<MessageWindow> window_;

    ClipboardEventCallback clipboard_event_callback_;

    std::string last_mime_type_;
    std::string last_data_;

    DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CLIPBOARD_H
