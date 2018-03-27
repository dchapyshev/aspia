//
// PROJECT:         Aspia
// FILE:            host/console_session_watcher.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
#define _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H

#include <memory>

#include "base/message_window.h"

namespace aspia {

class ConsoleSessionWatcher
{
public:
    ConsoleSessionWatcher() = default;
    ~ConsoleSessionWatcher();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnSessionDetached() = 0;
        virtual void OnSessionAttached(uint32_t session_id) = 0;
    };

    // Starts monitoring changes to the current console session.
    // Returns true if tracking could be started and false if failed.
    bool StartWatching(Delegate* delegate);

    // Stops monitoring.
    void StopWatching();

private:
    bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    std::unique_ptr<MessageWindow> window_;
    Delegate* delegate_ = nullptr;

    Q_DISABLE_COPY(ConsoleSessionWatcher)
};

} // namespace aspia

#endif // _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
