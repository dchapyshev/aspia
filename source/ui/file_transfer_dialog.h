//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_transfer_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER_DIALOG_H

#include "base/message_loop/message_loop_proxy.h"
#include "client/file_reply_receiver.h"
#include "client/file_request_sender_proxy.h"
#include "client/file_transfer.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class FileTransferDialog :
    public CDialogImpl<FileTransferDialog>,
    public FileTransfer::Delegate
{
public:
    enum { IDD = IDD_FILE_TRANSFER };

    enum class Mode { UPLOAD, DOWNLOAD };

    FileTransferDialog(Mode mode,
                       std::shared_ptr<FileRequestSenderProxy> sender,
                       const FilePath& source_path,
                       const FilePath& target_path,
                       const FileTransfer::FileList& file_list);
    ~FileTransferDialog() = default;

private:
    BEGIN_MSG_MAP(FileTransferDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    // FileTransfer::Delegate implementation.
    void OnTransferStarted(uint64_t size) final;
    void OnTransferComplete() final;
    void OnFileOperationFailure(const FilePath& file_path,
                                proto::RequestStatus status,
                                FileTransfer::ActionCallback callback) final;
    void OnObjectTransferStarted(const FilePath& object_path, uint64_t object_size) final;
    void OnObjectTransfer(uint64_t left_size) final;

    std::shared_ptr<MessageLoopProxy> runner_;
    std::shared_ptr<FileRequestSenderProxy> sender_;

    const FilePath& source_path_;
    const FilePath& target_path_;
    uint64_t total_size_ = 0;
    uint64_t total_transferred_size_ = 0;
    uint64_t object_size_ = 0;
    uint64_t object_transferred_size_ = 0;
    const FileTransfer::FileList& file_list_;
    const Mode mode_;

    std::unique_ptr<FileTransfer> file_transfer_;

    CProgressBarCtrl total_progress_;
    CProgressBarCtrl current_progress_;
    CEdit current_item_edit_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER_DIALOG_H
