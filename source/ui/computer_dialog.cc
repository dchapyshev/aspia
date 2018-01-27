//
// PROJECT:         Aspia
// FILE:            ui/computer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/computer_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "base/strings/unicode.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialog::ComputerDialog(const proto::Computer& computer)
    : computer_(computer)
{
    // Nothing
}

const proto::Computer& ComputerDialog::GetComputer() const
{
    return computer_;
}

LRESULT ComputerDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    DlgResize_Init();

    if (!computer_.name().empty())
    {
        GetDlgItem(IDC_NAME_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_.name()).c_str());
    }

    const proto::ClientConfig& client_config = computer_.client_config();

    if (!client_config.address().empty())
    {
        GetDlgItem(IDC_SERVER_ADDRESS_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_.name()).c_str());
    }

    if (client_config.port() == 0)
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort, FALSE);
        CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);
    }
    else
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, client_config.port(), FALSE);

        if (client_config.port() == kDefaultHostTcpPort)
            CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);
    }

    if (!computer_.comment().empty())
    {
        GetDlgItem(IDC_COMMENT_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_.comment()).c_str());
    }

    CComboBox combobox(GetDlgItem(IDC_SESSION_TYPE_COMBO));

    auto add_session = [&](UINT resource_id, proto::auth::SessionType session_type)
    {
        CString text;
        text.LoadStringW(resource_id);

        const int item_index = combobox.AddString(text);
        combobox.SetItemData(item_index, session_type);
    };

    add_session(IDS_SESSION_TYPE_DESKTOP_MANAGE, proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(IDS_SESSION_TYPE_DESKTOP_VIEW, proto::auth::SESSION_TYPE_DESKTOP_VIEW);
    add_session(IDS_SESSION_TYPE_FILE_TRANSFER, proto::auth::SESSION_TYPE_FILE_TRANSFER);
    add_session(IDS_SESSION_TYPE_SYSTEM_INFO, proto::auth::SESSION_TYPE_SYSTEM_INFO);
    add_session(IDS_SESSION_TYPE_POWER_MANAGE, proto::auth::SESSION_TYPE_POWER_MANAGE);

    // Select first item.
    combobox.SetCurSel(0);

    return FALSE;
}

LRESULT ComputerDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT ComputerDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CEdit name_edit(GetDlgItem(IDC_NAME_EDIT));

    int name_length = name_edit.GetWindowTextLengthW();
    if (name_length > kMaxNameLength)
    {
        CString message;
        message.LoadStringW(IDS_TOO_LONG_NAME_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else if (name_length < kMinNameLength)
    {
        CString message;
        message.LoadStringW(IDS_NAME_CANT_BE_EMPTY_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else
    {
        WCHAR name[kMaxNameLength + 1];
        name_edit.GetWindowTextW(name, _countof(name));

        computer_.set_name(UTF8fromUNICODE(name));
    }

    CEdit comment_edit(GetDlgItem(IDC_COMMENT_EDIT));
    if (comment_edit.GetWindowTextLengthW() > kMaxCommentLength)
    {
        CString message;
        message.LoadStringW(IDS_TOO_LONG_COMMENT_ERROR);
        MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
        return 0;
    }
    else
    {
        WCHAR comment[kMaxCommentLength + 1];
        comment_edit.GetWindowTextW(comment, _countof(comment));
        computer_.set_comment(UTF8fromUNICODE(comment));
    }

    EndDialog(IDOK);
    return 0;
}

LRESULT ComputerDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

} // namespace aspia
