//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_H
#define _ASPIA_UI__FILE_MANAGER_H

#include "base/message_loop/message_loop_thread.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/base/child_window.h"
#include "ui/base/splitter.h"
#include "ui/file_manager_panel.h"

namespace aspia {

class FileManager :
    public ChildWindow,
    private MessageLoopThread::Delegate,
    private FileManagerPanel::Delegate
{
public:
    using PanelType = FileManagerPanel::Type;

    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
        virtual void OnDriveListRequest(PanelType panel_type) = 0;
        virtual void OnDirectoryListRequest(PanelType panel_type, const std::wstring& path) = 0;
        virtual void OnSendFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
        virtual void OnRecieveFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
    };

    FileManager(Delegate* delegate);
    ~FileManager();

    void AddDriveItem(PanelType panel_type,
                      proto::DriveListItem::Type drive_type,
                      const std::wstring& drive_path,
                      const std::wstring& drive_name);

    void AddDirectoryItem(PanelType panel_type,
                          proto::DirectoryListItem::Type item_type,
                          const std::wstring& item_name,
                          uint64_t item_size);

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // FileManagerPanel::Delegate implementation.
    void OnDriveListRequest(PanelType panel_type) override;

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
