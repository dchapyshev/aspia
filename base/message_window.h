//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_WINDOW_H
#define _ASPIA_BASE__MESSAGE_WINDOW_H

#include "base/macros.h"
#include "base/thread.h"

namespace aspia {

class MessageWindow : private Thread
{
public:
    MessageWindow();
    virtual ~MessageWindow();

    void CreateMessageWindow();
    void DestroyMessageWindow();
    HWND GetMessageWindowHandle();

    virtual void OnMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) = 0;

private:
    void Worker() override;
    void OnStop() override;

    bool RegisterWindowClass(HINSTANCE instance);

    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND hwnd_;

    DISALLOW_COPY_AND_ASSIGN(MessageWindow);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_WINDOW_H
