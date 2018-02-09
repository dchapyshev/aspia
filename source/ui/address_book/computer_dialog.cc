//
// PROJECT:         Aspia
// FILE:            ui/address_book/computer_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/computer_dialog.h"

#include <atlctrls.h>
#include <atlmisc.h>

#include "base/strings/unicode.h"
#include "codec/video_helpers.h"
#include "network/network_client_tcp.h"
#include "ui/desktop/settings_dialog.h"
#include "ui/ui_util.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialog::ComputerDialog()
{
    // Load default config.
    computer_.set_port(kDefaultHostTcpPort);
    computer_.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);

    proto::desktop::Config* desktop_session_config = computer_.mutable_desktop_session();

    desktop_session_config->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                                      proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_session_config->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_session_config->set_update_interval(30);
    desktop_session_config->set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(),
                              desktop_session_config->mutable_pixel_format());
}

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

    if (!computer_.address().empty())
    {
        GetDlgItem(IDC_SERVER_ADDRESS_EDIT).SetWindowTextW(
            UNICODEfromUTF8(computer_.address()).c_str());
    }

    CEdit port_edit(GetDlgItem(IDC_SERVER_PORT_EDIT));
    port_edit.SetLimitText(5);

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, computer_.port(), FALSE);

    if (computer_.port() == kDefaultHostTcpPort)
    {
        port_edit.SetReadOnly(TRUE);
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

        if (session_type == computer_.session_type())
            combobox.SetCurSel(item_index);
    };

    add_session(IDS_SESSION_TYPE_DESKTOP_MANAGE, proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
    add_session(IDS_SESSION_TYPE_DESKTOP_VIEW, proto::auth::SESSION_TYPE_DESKTOP_VIEW);
    add_session(IDS_SESSION_TYPE_FILE_TRANSFER, proto::auth::SESSION_TYPE_FILE_TRANSFER);
    add_session(IDS_SESSION_TYPE_SYSTEM_INFO, proto::auth::SESSION_TYPE_SYSTEM_INFO);

    UpdateCurrentSessionType(computer_.session_type());

    return FALSE;
}

LRESULT ComputerDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT ComputerDialog::OnDefaultPortClicked(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));

    if (IsDlgButtonChecked(IDC_SERVER_DEFAULT_PORT_CHECK) == BST_CHECKED)
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
        port.SetReadOnly(TRUE);
    }
    else
    {
        port.SetReadOnly(FALSE);
    }

    return 0;
}

LRESULT ComputerDialog::OnSettingsButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    switch (computer_.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        {
            SettingsDialog dialog(computer_.session_type(), computer_.desktop_session());

            if (dialog.DoModal(*this) == IDOK)
            {
                computer_.mutable_desktop_session()->CopyFrom(dialog.Config());
            }
        }
        break;

        default:
            break;
    }

    return 0;
}

LRESULT ComputerDialog::OnOkButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    std::wstring name = GetWindowString(GetDlgItem(IDC_NAME_EDIT));
    if (name.length() > kMaxNameLength)
    {
        ShowErrorMessage(IDS_AB_TOO_LONG_NAME_ERROR);
        return 0;
    }
    else if (name.length() < kMinNameLength)
    {
        ShowErrorMessage(IDS_AB_NAME_CANT_BE_EMPTY_ERROR);
        return 0;
    }
    else
    {
        computer_.set_name(UTF8fromUNICODE(name));
    }

    std::wstring comment = GetWindowString(GetDlgItem(IDC_COMMENT_EDIT));
    if (comment.length() > kMaxCommentLength)
    {
        ShowErrorMessage(IDS_AB_TOO_LONG_COMMENT_ERROR);
        return 0;
    }
    else
    {
        computer_.set_comment(UTF8fromUNICODE(comment));
    }

    std::wstring address = GetWindowString(GetDlgItem(IDC_SERVER_ADDRESS_EDIT));
    if (!NetworkClientTcp::IsValidHostName(address))
    {
        ShowErrorMessage(IDS_CONN_STATUS_INVALID_ADDRESS);
        return 0;
    }

    uint32_t port = static_cast<uint32_t>(GetDlgItemInt(IDC_SERVER_PORT_EDIT, nullptr, FALSE));
    if (!NetworkClientTcp::IsValidPort(port))
    {
        ShowErrorMessage(IDS_CONN_STATUS_INVALID_PORT);
        return 0;
    }

    computer_.set_address(UTF8fromUNICODE(address));
    computer_.set_port(port);

    EndDialog(IDOK);
    return 0;
}

LRESULT ComputerDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT ComputerDialog::OnSessionTypeChanged(
    WORD /* notify_code */, WORD /* control_id */, HWND control, BOOL& /* handled */)
{
    CComboBox combo(control);
    UpdateCurrentSessionType(static_cast<proto::auth::SessionType>(
        combo.GetItemData(combo.GetCurSel())));
    return 0;
}

void ComputerDialog::UpdateCurrentSessionType(proto::auth::SessionType session_type)
{
    computer_.set_session_type(session_type);

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            GetDlgItem(IDC_SETTINGS_BUTTON).EnableWindow(TRUE);
            break;

        default:
            GetDlgItem(IDC_SETTINGS_BUTTON).EnableWindow(FALSE);
            break;
    }
}

void ComputerDialog::ShowErrorMessage(UINT string_id)
{
    CString message;
    message.LoadStringW(string_id);
    MessageBoxW(message, nullptr, MB_OK | MB_ICONWARNING);
}

} // namespace aspia
