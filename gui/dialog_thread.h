//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/dialog_thread.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__DIALOG_THREAD_H
#define _ASPIA_GUI__DIALOG_THREAD_H

#include <memory>

#include "base/thread.h"

namespace aspia {

template <class T>
class DialogThread : public Thread
{
public:
    explicit DialogThread(std::unique_ptr<typename T> window) :
        window_(std::move(window))
    {
        Start();
    }

    ~DialogThread()
    {
        Stop();
        Wait();
    }

    void Worker() override
    {
        window_->DoModal(NULL);
    }

    void OnStop() override
    {
        window_->PostMessageW(WM_CLOSE);
    }

private:
    std::unique_ptr<typename T> window_;
};

} // namespace aspia

#endif // _ASPIA_GUI__DIALOG_THREAD_H
