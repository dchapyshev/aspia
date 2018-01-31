//
// PROJECT:         Aspia
// FILE:            ui/address_book/computer_group_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/computer_group_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "base/strings/unicode.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerGroupDialog::ComputerGroupDialog(const proto::ComputerGroup& computer_group)
    : computer_group_(computer_group)
{
    // Nothing
}

const proto::ComputerGroup& ComputerGroupDialog::GetComputerGroup() const
{
    return computer_group_;
}

LRESULT ComputerGroupDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    if (!computer_group_.name().empty())
    {
        GetDlgItem(IDC_NAME_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_group_.name()).c_str());
    }

    if (!computer_group_.comment().empty())
    {
        GetDlgItem(IDC_COMMENT_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_group_.comment()).c_str());
    }

    return FALSE;
}

LRESULT ComputerGroupDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT ComputerGroupDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CEdit name_edit(GetDlgItem(IDC_NAME_EDIT));

    int name_length = name_edit.GetWindowTextLengthW();
    if (name_length > kMaxNameLength)
    {
        CString message;
        message.LoadStringW(IDS_AB_TOO_LONG_NAME_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else if (name_length < kMinNameLength)
    {
        CString message;
        message.LoadStringW(IDS_AB_NAME_CANT_BE_EMPTY_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else
    {
        WCHAR name[kMaxNameLength + 1];
        name_edit.GetWindowTextW(name, _countof(name));
        computer_group_.set_name(UTF8fromUNICODE(name));
    }

    CEdit comment_edit(GetDlgItem(IDC_COMMENT_EDIT));
    if (comment_edit.GetWindowTextLengthW() > kMaxCommentLength)
    {
        CString message;
        message.LoadStringW(IDS_AB_TOO_LONG_COMMENT_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else
    {
        WCHAR comment[kMaxCommentLength + 1];
        comment_edit.GetWindowTextW(comment, _countof(comment));
        computer_group_.set_comment(UTF8fromUNICODE(comment));
    }

    EndDialog(IDOK);
    return 0;
}

LRESULT ComputerGroupDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

} // namespace aspia
