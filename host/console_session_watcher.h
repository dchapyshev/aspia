//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_watcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
#define _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H

#include "aspia_config.h"

#include <functional>

#include "base/message_window.h"

namespace aspia {

class ConsoleSessionWatcher : private MessageWindow
{
public:
    ConsoleSessionWatcher();
    ~ConsoleSessionWatcher();

    //
    // «апускает отслеживание изменений текущей консольной сессии.
    // ¬озвращаетс€ true, если удалось запустить отслеживание и false, если не удалось.
    // ћетод вызывает указанный callback с ID текущей сессии.
    //
    void StartSessionWatching();

    // ќстанавливает отслеживание.
    void StopSessionWatching();

    void DetachSession();

    virtual void OnSessionDetached() = 0;
    virtual void OnSessionAttached(uint32_t session_id) = 0;

private:
    void OnMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;

    DISALLOW_COPY_AND_ASSIGN(ConsoleSessionWatcher);
};

} // namespace aspia

#endif // _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
