//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/clipboard.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__CLIPBOARD_H
#define _ASPIA_PROTOCOL__CLIPBOARD_H

#include "aspia_config.h"

#include <functional>
#include <string>

#include "base/message_window.h"
#include "proto/proto.pb.h"

namespace aspia {

class Clipboard : private MessageWindow
{
public:
    Clipboard();
    ~Clipboard();

    typedef std::function<void(const proto::ClipboardEvent&)> ClipboardEventCallback;

    // Callback вызывается при наличии исходящего буфера обмена.
    void StartExchange(const ClipboardEventCallback& clipboard_event_callback);
    void StopExchange();

    // Прием входящего буфера обмена.
    void InjectClipboardEvent(const proto::ClipboardEvent& event);

private:
    void OnMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;
    void OnClipboardUpdate();

private:
    ClipboardEventCallback clipboard_event_callback_;

    std::string last_mime_type_;
    std::string last_data_;

    DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

} // namespace aspia

#endif // _ASPIA_PROTOCOL__CLIPBOARD_H
