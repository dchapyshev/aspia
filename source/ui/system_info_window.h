//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO_WINDOW_H
#define _ASPIA_UI__SYSTEM_INFO_WINDOW_H

#include "base/message_loop/message_loop_thread.h"
#include "ui/base/splitter.h"

namespace aspia {

class SystemInfoWindow
    : public CWindowImpl<SystemInfoWindow, CWindow, CFrameWinTraits>,
      private MessageLoopThread::Delegate,
      private MessageLoop::Dispatcher
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnWindowClose() = 0;
    };

    SystemInfoWindow(Delegate* delegate);
    ~SystemInfoWindow();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnThreadRunning(MessageLoop* message_loop) override;
    void OnAfterThreadRunning() override;

    // MessageLoop::Dispatcher implementation.
    bool Dispatch(const NativeEvent& event) override;

    BEGIN_MSG_MAP(SystemInfoWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;

    VerticalSplitter splitter_;

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(SystemInfoWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO_WINDOW_H
