//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_io.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H

#include "base/macros.h"
#include "base/scoped_object.h"
#include "base/message_loop/message_pump_win.h"

#include <list>

namespace aspia {

class MessagePumpForIO : public MessagePumpWin
{
public:
    MessagePumpForIO();
    virtual ~MessagePumpForIO();

    struct IOContext;

    class IOHandler
    {
    public:
        virtual ~IOHandler()
        {
        }
        // This will be called once the pending IO operation associated with
        // |context| completes. |error| is the Win32 error code of the IO operation
        // (ERROR_SUCCESS if there was no error). |bytes_transfered| will be zero
        // on error.
        virtual void OnIOCompleted(IOContext* context, DWORD bytes_transfered,
                                   DWORD error) = 0;
    };

    struct IOContext
    {
        OVERLAPPED overlapped;
        IOHandler* handler;
    };

    // MessagePump methods:
    virtual void ScheduleWork() override;

    // Register the handler to be used when asynchronous IO for the given file
    // completes. The registration persists as long as |file_handle| is valid, so
    // |handler| must be valid as long as there is pending IO for the given file.
    void RegisterIOHandler(HANDLE file_handle, IOHandler* handler);

    // Waits for the next IO completion that should be processed by |filter|, for
    // up to |timeout| milliseconds. Return true if any IO operation completed,
    // regardless of the involved handler, and false if the timeout expired. If
    // the completion port received any message and the involved IO handler
    // matches |filter|, the callback is called before returning from this code;
    // if the handler is not the one that we are looking for, the callback will
    // be postponed for another time, so reentrancy problems can be avoided.
    // External use of this method should be reserved for the rare case when the
    // caller is willing to allow pausing regular task dispatching on this thread.
    bool WaitForIOCompletion(DWORD timeout, IOHandler* filter);

private:
    struct IOItem
    {
        IOHandler* handler;
        IOContext* context;
        DWORD bytes_transfered;
        DWORD error;
    };

    void DoRunLoop() override;
    void WaitForWork();
    bool MatchCompletedIOItem(IOHandler* filter, IOItem* item);
    bool GetIOItem(DWORD timeout, IOItem* item);
    bool ProcessInternalIOItem(const IOItem& item);

    // The completion port associated with this thread.
    ScopedHandle port_;
    // This list will be empty almost always. It stores IO completions that have
    // not been delivered yet because somebody was doing cleanup.
    std::list<IOItem> completed_io_;

    DISALLOW_COPY_AND_ASSIGN(MessagePumpForIO);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H
