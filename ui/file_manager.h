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

class UiFileManager :
    public UiChildWindow,
    private MessageLoopThread::Delegate,
    private UiFileManagerPanel::Delegate
{
public:
    using PanelType = UiFileManagerPanel::PanelType;

    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
        virtual void OnDriveListRequest(PanelType panel_type) = 0;

        virtual void OnDirectoryListRequest(PanelType panel_type,
                                            const std::string& path,
                                            const std::string& item) = 0;

        virtual void OnCreateDirectoryRequest(PanelType panel_type,
                                              const std::string& path,
                                              const std::string& name) = 0;

        virtual void OnRenameRequest(PanelType panel_type,
                                     const std::string& path,
                                     const std::string& old_name,
                                     const std::string& new_name) = 0;

        virtual void OnRemoveRequest(PanelType panel_type,
                                     const std::string& path,
                                     const std::string& item_name) = 0;

        virtual void OnSendFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
        virtual void OnRecieveFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
    };

    UiFileManager(Delegate* delegate);
    ~UiFileManager();

    void ReadStatusCode(proto::Status status);

    void ReadDriveList(PanelType panel_type,
                       std::unique_ptr<proto::DriveList> drive_list);

    void ReadDirectoryList(PanelType panel_type,
                           std::unique_ptr<proto::DirectoryList> directory_list);

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // UiFileManagerPanel::Delegate implementation.
    void OnDriveListRequest(PanelType panel_type) override;

    void OnDirectoryListRequest(PanelType panel_type,
                                const std::string& path,
                                const std::string& item) override;

    void OnCreateDirectoryRequest(PanelType panel_type,
                                  const std::string& path,
                                  const std::string& name) override;

    void OnRenameRequest(PanelType panel_type,
                         const std::string& path,
                         const std::string& old_path,
                         const std::string& new_path) override;

    void OnRemoveRequest(PanelType panel_type,
                         const std::string& path,
                         const std::string& item_name) override;

    // UiChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnGetMinMaxInfo(LPMINMAXINFO mmi);
    void OnClose();

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;

    UiFileManagerPanel local_panel_;
    UiFileManagerPanel remote_panel_;
    UiSplitter splitter_;

    DISALLOW_COPY_AND_ASSIGN(UiFileManager);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_H
