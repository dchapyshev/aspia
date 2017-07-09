//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER_DIALOG_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "client/file_reply_receiver.h"
#include "client/file_request_sender_proxy.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiFileTransferDialog :
    public CDialogImpl<UiFileTransferDialog>,
    public FileReplyReceiver
{
public:
    enum { IDD = IDD_FILE_TRANSFER };

    UiFileTransferDialog(std::shared_ptr<FileRequestSenderProxy> sender,
                         const FilePath& path,
                         const std::vector<proto::FileList::Item>& file_list);
    ~UiFileTransferDialog() = default;

private:
    BEGIN_MSG_MAP(UiFileTransferDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void OnDriveListRequestReply(std::unique_ptr<proto::DriveList> drive_list) final;
    void OnDriveListRequestFailure(proto::RequestStatus status) final;

    void OnFileListRequestReply(const FilePath& path,
                                std::unique_ptr<proto::FileList> file_list) final;

    void OnFileListRequestFailure(const FilePath& path,
                                  proto::RequestStatus status) final;

    void OnDirectorySizeRequestReply(const FilePath& path, uint64_t size) final;

    void OnDirectorySizeRequestFailure(const FilePath& path,
                                       proto::RequestStatus status) final;

    void OnCreateDirectoryRequestReply(const FilePath& path,
                                       proto::RequestStatus status) final;

    void OnRemoveRequestReply(const FilePath& path,
                              proto::RequestStatus status) final;

    void OnRenameRequestReply(const FilePath& old_name,
                              const FilePath& new_name,
                              proto::RequestStatus status) final;

    std::shared_ptr<FileRequestSenderProxy> sender_;

    const FilePath& path_;
    const std::vector<proto::FileList::Item>& file_list_;

    DISALLOW_COPY_AND_ASSIGN(UiFileTransferDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER_DIALOG_H
