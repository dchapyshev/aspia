//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_H
#define _ASPIA_UI__FILE_MANAGER_H

#include "base/message_loop/message_loop_thread.h"
#include "client/file_request_sender_proxy.h"
#include "ui/base/splitter.h"
#include "ui/file_manager_panel.h"

namespace aspia {

class UiFileManager :
    public CWindowImpl<UiFileManager, CWindow, CFrameWinTraits>,
    private MessageLoopThread::Delegate,
    private UiFileManagerPanel::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnWindowClose() = 0;
    };

    UiFileManager(std::shared_ptr<FileRequestSenderProxy> local_sender,
                  std::shared_ptr<FileRequestSenderProxy> remote_sender,
                  Delegate* delegate);
    ~UiFileManager();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // UiFileManagerPanel::Delegate implementation.
    void SendFiles(UiFileManagerPanel::PanelType panel_type,
                   const FilePath& source_path,
                   const FileTransfer::FileList& file_list) override;

    BEGIN_MSG_MAP(UiFileManager)
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

    std::shared_ptr<FileRequestSenderProxy> local_sender_;
    std::shared_ptr<FileRequestSenderProxy> remote_sender_;

    Delegate* delegate_;

    UiFileManagerPanel local_panel_;
    UiFileManagerPanel remote_panel_;
    UiVerticalSplitter splitter_;

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiFileManager);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_H
