//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_H
#define _ASPIA_UI__FILE_MANAGER_H

#include "base/message_loop/message_loop_thread.h"
#include "ui/base/child_window.h"
#include "ui/base/splitter.h"
#include "ui/file_manager_panel.h"

namespace aspia {

class FileManager :
    public ChildWindow,
    private MessageLoopThread::Delegate
{
public:
    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
    };

    FileManager(Delegate* delegate);
    ~FileManager();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnGetMinMaxInfo(LPMINMAXINFO mmi);
    void OnClose();

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;

    FileManagerPanel local_panel_;
    FileManagerPanel remote_panel_;
    Splitter splitter_;

    DISALLOW_COPY_AND_ASSIGN(FileManager);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_H
