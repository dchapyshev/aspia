//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/main_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/main_dialog.h"

#include <atlmisc.h>
#include <uxtheme.h>

#include "base/version_helpers.h"
#include "base/process/process_helpers.h"
#include "base/strings/unicode.h"
#include "base/scoped_clipboard.h"
#include "client/client_config.h"
#include "host/host_session_launcher.h"
#include "host/host_service.h"
#include "network/network_adapter_enumerator.h"
#include "ui/desktop/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/users_dialog.h"

namespace aspia {

void MainDialog::InitIpList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(list, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list.SetExtendedListViewStyle(ex_style);

    const int column_index = list.AddColumn(L"", 0);

    CRect list_rect;
    list.GetClientRect(list_rect);
    list.SetColumnWidth(column_index, list_rect.Width() - GetSystemMetrics(SM_CXVSCROLL));
}

void MainDialog::UpdateIpList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    list.DeleteAllItems();

    for (NetworkAdapterEnumerator adapter; !adapter.IsAtEnd(); adapter.Advance())
    {
        for (NetworkAdapterEnumerator::IpAddressEnumerator address(adapter);
             !address.IsAtEnd();
             address.Advance())
        {
            list.AddItem(list.GetItemCount(),
                         0,
                         UNICODEfromUTF8(address.GetIpAddress()).c_str(),
                         0);
        }
    }
}

int MainDialog::AddSessionType(CComboBox& combobox,
                               UINT string_resource_id,
                               proto::SessionType session_type)
{
    CString text;
    text.LoadStringW(string_resource_id);

    const int item_index = combobox.AddString(text);
    combobox.SetItemData(item_index, session_type);

    return item_index;
}

void MainDialog::InitSessionTypesCombo()
{
    CComboBox combobox(GetDlgItem(IDC_SESSION_TYPE_COMBO));

    const int first_item = AddSessionType(combobox,
                                          IDS_SESSION_TYPE_DESKTOP_MANAGE,
                                          proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_DESKTOP_VIEW,
                   proto::SessionType::SESSION_TYPE_DESKTOP_VIEW);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_FILE_TRANSFER,
                   proto::SessionType::SESSION_TYPE_FILE_TRANSFER);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_SYSTEM_INFO,
                   proto::SessionType::SESSION_TYPE_SYSTEM_INFO);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_POWER_MANAGE,
                   proto::SessionType::SESSION_TYPE_POWER_MANAGE);

    combobox.SetCurSel(first_item);
}

proto::SessionType MainDialog::GetSelectedSessionType() const
{
    CComboBox combo(GetDlgItem(IDC_SESSION_TYPE_COMBO));
    return static_cast<proto::SessionType>(combo.GetItemData(combo.GetCurSel()));
}

LRESULT MainDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    CSize small_icon_size(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   small_icon_size.cx,
                                   small_icon_size.cy);
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(big_icon_, TRUE);

    refresh_icon_ = AtlLoadIconImage(IDI_REFRESH,
                                     LR_CREATEDIBSECTION,
                                     small_icon_size.cx,
                                     small_icon_size.cy);
    CButton(GetDlgItem(IDC_UPDATE_IP_LIST_BUTTON)).SetIcon(refresh_icon_);

    main_menu_ = AtlLoadMenu(IDR_MAIN);
    SetMenu(main_menu_);

    CString tray_tooltip;
    tray_tooltip.LoadStringW(IDS_APPLICATION_NAME);

    AddTrayIcon(small_icon_, tray_tooltip, IDR_TRAY);
    SetDefaultTrayMenuItem(ID_SHOWHIDE);

    InitIpList();
    UpdateIpList();
    InitSessionTypesCombo();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
    CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    CEdit(GetDlgItem(IDC_SERVER_PORT_EDIT)).SetReadOnly(TRUE);

    const bool host_service_installed = HostService::IsInstalled();

    if (!IsCallerHasAdminRights())
    {
        main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
    }
    else
    {
        if (host_service_installed)
            main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        else
            main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
    }

    if (host_service_installed)
        GetDlgItem(IDC_START_SERVER_BUTTON).EnableWindow(FALSE);

    UpdateSessionType();

    const DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    const DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    GetDlgItem(IDC_SERVER_ADDRESS_EDIT).SetFocus();

    return FALSE;
}

LRESULT MainDialog::OnDefaultPortClicked(WORD notify_code, WORD control_id, HWND control,
                                         BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

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

LRESULT MainDialog::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    UNUSED_PARAMETER(message);
    UNUSED_PARAMETER(wparam);
    UNUSED_PARAMETER(lparam);
    UNUSED_PARAMETER(handled);

    host_pool_.reset();
    client_pool_.reset();
    DestroyWindow();
    PostQuitMessage(0);
    return 0;
}

void MainDialog::StopHostMode()
{
    if (host_pool_)
    {
        CString text;
        text.LoadStringW(IDS_START);
        SetDlgItemTextW(IDC_START_SERVER_BUTTON, text);

        host_pool_.reset();
    }
}

LRESULT MainDialog::OnStartServerButton(WORD notify_code, WORD control_id, HWND control,
                                        BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    if (host_pool_)
    {
        StopHostMode();
        return 0;
    }

    host_pool_ = std::make_unique<HostPool>(MessageLoopProxy::Current());

    CString text;
    text.LoadStringW(IDS_STOP);
    SetDlgItemTextW(IDC_START_SERVER_BUTTON, text);

    return 0;
}

LRESULT MainDialog::OnUpdateIpListButton(WORD notify_code, WORD control_id, HWND control,
                                         BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    UpdateIpList();
    return 0;
}

void MainDialog::UpdateSessionType()
{
    switch (GetSelectedSessionType())
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
        {
            config_.SetDefaultDesktopSessionConfig();
            GetDlgItem(IDC_SETTINGS_BUTTON).EnableWindow(TRUE);
        }
        break;

        default:
            GetDlgItem(IDC_SETTINGS_BUTTON).EnableWindow(FALSE);
            break;
    }
}

LRESULT MainDialog::OnSessionTypeChanged(WORD notify_code, WORD control_id, HWND control,
                                         BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    UpdateSessionType();
    return 0;
}

LRESULT MainDialog::OnSettingsButton(WORD notify_code, WORD control_id, HWND control,
                                     BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    const proto::SessionType session_type = GetSelectedSessionType();

    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
        {
            SettingsDialog dialog(session_type, config_.desktop_session_config());

            if (dialog.DoModal(*this) == IDOK)
            {
                config_.mutable_desktop_session_config()->CopyFrom(dialog.Config());
            }
        }
        break;

        default:
            break;
    }

    return 0;
}

LRESULT MainDialog::OnConnectButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    const proto::SessionType session_type = GetSelectedSessionType();

    if (session_type != proto::SessionType::SESSION_TYPE_UNKNOWN)
    {
        WCHAR buffer[128] = { 0 };
        GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, buffer, _countof(buffer));

        config_.set_address(buffer);
        config_.set_port(static_cast<uint16_t>(GetDlgItemInt(IDC_SERVER_PORT_EDIT, nullptr, FALSE)));
        config_.set_session_type(session_type);

        if (!client_pool_)
            client_pool_ = std::make_unique<ClientPool>(MessageLoopProxy::Current());

        client_pool_->Connect(*this, config_);
    }

    return 0;
}

LRESULT MainDialog::OnHelpButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    CString url;
    url.LoadStringW(IDS_HELP_LINK);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT MainDialog::OnShowHideButton(WORD notify_code, WORD control_id, HWND control,
                                     BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    if (IsWindowVisible())
        ShowWindow(SW_HIDE);
    else
        ShowWindow(SW_SHOWNORMAL);

    return 0;
}

LRESULT MainDialog::OnInstallServiceButton(WORD notify_code, WORD control_id, HWND control,
                                           BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    if (host_pool_)
    {
        StopHostMode();
    }

    if (HostService::Install())
    {
        main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        GetDlgItem(IDC_START_SERVER_BUTTON).EnableWindow(FALSE);
    }

    return 0;
}

LRESULT MainDialog::OnRemoveServiceButton(WORD notify_code, WORD control_id, HWND control,
                                          BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    if (HostService::Remove())
    {
        main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        GetDlgItem(IDC_START_SERVER_BUTTON).EnableWindow(TRUE);
    }

    return 0;
}

void MainDialog::CopySelectedIp()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    const int selected_item = list.GetNextItem(-1, LVNI_SELECTED);
    if (selected_item == -1)
        return;

    WCHAR text[128] = { 0 };
    if (!list.GetItemText(selected_item, 0, text, _countof(text)))
        return;

    const size_t length = wcslen(text);
    if (!length)
        return;

    ScopedClipboard clipboard;
    if (!clipboard.Init(*this))
        return;

    clipboard.Empty();

    HGLOBAL text_global = GlobalAlloc(GMEM_MOVEABLE, (length + 1) * sizeof(WCHAR));
    if (!text_global)
        return;

    LPWSTR text_global_locked = reinterpret_cast<LPWSTR>(GlobalLock(text_global));
    if (!text_global_locked)
    {
        GlobalFree(text_global);
        return;
    }

    memcpy(text_global_locked, text, length * sizeof(WCHAR));
    text_global_locked[length] = 0;

    GlobalUnlock(text_global);

    clipboard.SetData(CF_UNICODETEXT, text_global);
}

LRESULT MainDialog::OnCopyButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    CopySelectedIp();
    return 0;
}

LRESULT MainDialog::OnIpListDoubleClick(int control_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(hdr);
    UNUSED_PARAMETER(handled);

    CopySelectedIp();
    return 0;
}

LRESULT MainDialog::OnIpListRightClick(int control_id, LPNMHDR hdr, BOOL& handled)
{
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(hdr);
    UNUSED_PARAMETER(handled);

    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    if (!list.GetSelectedCount())
        return 0;

    CMenu menu(AtlLoadMenu(IDR_IP_LIST));

    if (menu)
    {
        CPoint cursor_pos;

        if (GetCursorPos(&cursor_pos))
        {
            SetForegroundWindow(*this);

            CMenuHandle popup_menu(menu.GetSubMenu(0));
            if (popup_menu)
            {
                popup_menu.TrackPopupMenu(0, cursor_pos.x, cursor_pos.y, *this, nullptr);
            }
        }
    }

    return 0;
}

LRESULT MainDialog::OnSystemInfoButton(WORD notify_code, WORD control_id, HWND control,
                                       BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    LaunchSystemInfoProcess();
    return 0;
}

LRESULT MainDialog::OnExitButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    EndDialog(0);
    return 0;
}

LRESULT MainDialog::OnAboutButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    AboutDialog().DoModal(*this);
    return 0;
}

LRESULT MainDialog::OnUsersButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled)
{
    UNUSED_PARAMETER(notify_code);
    UNUSED_PARAMETER(control_id);
    UNUSED_PARAMETER(control);
    UNUSED_PARAMETER(handled);

    UsersDialog().DoModal(*this);
    return 0;
}

} // namespace aspia
