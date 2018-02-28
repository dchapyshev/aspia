//
// PROJECT:         Aspia
// FILE:            ui/file_transfer/file_action_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_TRANSFER__FILE_ACTION_DIALOG_H
#define _ASPIA_UI__FILE_TRANSFER__FILE_ACTION_DIALOG_H

#include "base/macros.h"
#include "client/file_transfer.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>

namespace aspia {

class FileActionDialog : public CDialogImpl<FileActionDialog>
{
public:
    enum { IDD = IDD_FILE_ACTION };

    FileActionDialog(const std::experimental::filesystem::path& path,
                     proto::file_transfer::Status status);
    ~FileActionDialog() = default;

    FileAction GetAction() const { return action_; }

private:
    BEGIN_MSG_MAP(FileReplaceDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_REPLACE_BUTTON, OnReplaceButton)
        COMMAND_ID_HANDLER(IDC_REPLACE_ALL_BUTTON, OnReplaceAllButton)
        COMMAND_ID_HANDLER(IDC_SKIP_BUTTON, OnSkipButton)
        COMMAND_ID_HANDLER(IDC_SKIP_ALL_BUTTON, OnSkipAllButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnReplaceButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnReplaceAllButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSkipButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSkipAllButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    const std::experimental::filesystem::path path_;
    const proto::file_transfer::Status status_;
    FileAction action_ = FileAction::ABORT;

    DISALLOW_COPY_AND_ASSIGN(FileActionDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_TRANSFER__FILE_ACTION_DIALOG_H
