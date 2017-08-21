//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager_panel.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_PANEL_H
#define _ASPIA_UI__FILE_MANAGER_PANEL_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "client/file_request_sender_proxy.h"
#include "client/file_transfer.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/file_toolbar.h"
#include "ui/file_list_window.h"
#include "ui/drive_list_window.h"
#include "ui/resource.h"

namespace aspia {

class FileManagerPanel :
    public CWindowImpl<FileManagerPanel, CWindow>,
    public FileReplyReceiver
{
public:
    enum class Type { LOCAL, REMOTE };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void SendFiles(Type type,
                               const FilePath& source_path,
                               const FileTaskQueueBuilder::FileList& file_list) = 0;
    };

    FileManagerPanel(Type type,
                     std::shared_ptr<FileRequestSenderProxy> sender,
                     Delegate* delegate);
    virtual ~FileManagerPanel() = default;

    FilePath GetCurrentPath() const;
    void Refresh();

private:
    static const int kDriveListControl = 101;
    static const int kFileListControl = 102;

    BEGIN_MSG_MAP(FileManagerPanel)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

        NOTIFY_HANDLER(kDriveListControl, CBEN_ENDEDITW, OnDriveEndEdit)
        NOTIFY_HANDLER(kFileListControl, NM_DBLCLK, OnListDoubleClock)
        NOTIFY_HANDLER(kFileListControl, LVN_ENDLABELEDIT, OnListEndLabelEdit)
        NOTIFY_HANDLER(kFileListControl, LVN_ITEMCHANGED, OnListItemChanged)

        COMMAND_HANDLER(kDriveListControl, CBN_SELCHANGE, OnDriveChange)
        COMMAND_ID_HANDLER(ID_FOLDER_UP, OnFolderUp)
        COMMAND_ID_HANDLER(ID_FOLDER_ADD, OnFolderAdd)
        COMMAND_ID_HANDLER(ID_REFRESH, OnRefresh)
        COMMAND_ID_HANDLER(ID_DELETE, OnRemove)
        COMMAND_ID_HANDLER(ID_HOME, OnHome)
        COMMAND_ID_HANDLER(ID_SEND, OnSend)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDriveEndEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnListDoubleClock(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnListEndLabelEdit(int ctrl_id, LPNMHDR hdr, BOOL& handled);
    LRESULT OnListItemChanged(int ctrl_id, LPNMHDR hdr, BOOL& handled);

    LRESULT OnDriveChange(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnFolderUp(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnFolderAdd(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnRefresh(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnRemove(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnHome(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);
    LRESULT OnSend(WORD code, WORD ctrl_id, HWND ctrl, BOOL& handled);

    // FileReplyReceiver implementation.
    void OnDriveListReply(std::shared_ptr<proto::DriveList> drive_list,
                          proto::RequestStatus status) override;
    void OnFileListReply(const FilePath& path,
                         std::shared_ptr<proto::FileList> file_list,
                         proto::RequestStatus status) override;
    void OnCreateDirectoryReply( const FilePath& path, proto::RequestStatus status) override;
    void OnRenameReply(const FilePath& old_name,
                       const FilePath& new_name,
                       proto::RequestStatus status) override;

    void MoveToDrive(int object_index);

    const Type type_;
    Delegate* delegate_;
    std::shared_ptr<FileRequestSenderProxy> sender_;

    CStatic title_;
    DriveListWindow drive_list_;
    FileListWindow file_list_;
    FileToolBar toolbar_;
    CStatic status_;

    DISALLOW_COPY_AND_ASSIGN(FileManagerPanel);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_PANEL_H
