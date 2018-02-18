//
// PROJECT:         Aspia
// FILE:            ui/main_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/main_dialog.h"

#include <atlmisc.h>
#include <uxtheme.h>

#include "base/files/base_paths.h"
#include "base/process/process_helpers.h"
#include "base/strings/unicode.h"
#include "base/version_helpers.h"
#include "base/scoped_clipboard.h"
#include "base/settings_manager.h"
#include "host/host_session_launcher.h"
#include "host/host_service.h"
#include "network/network_adapter_enumerator.h"
#include "network/url.h"
#include "ui/desktop/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/users_dialog.h"
#include "ui/ui_util.h"
#include "command_line_switches.h"

namespace aspia {

namespace {

struct LangList
{
    LANGID langid;
    UINT command_id;
} constexpr kLangList[] =
{
    { MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH),            ID_DUTCH_LANGUAGE   },
    { MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),     ID_ENGLISH_LANGUAGE },
    { MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA), ID_RUSSIAN_LANGUAGE }
};

LANGID CommandIdToLangId(UINT command_id)
{
    for (size_t i = 0; i < _countof(kLangList); ++i)
    {
        if (kLangList[i].command_id == command_id)
            return kLangList[i].langid;
    }

    return 0;
}

UINT LangIdToCommandId(LANGID langid)
{
    for (size_t i = 0; i < _countof(kLangList); ++i)
    {
        if (kLangList[i].langid == langid)
            return kLangList[i].command_id;
    }

    return 0;
}

} // namespace

bool MainDialog::Dispatch(const NativeEvent& event)
{
    if (!accelerator_.TranslateAcceleratorW(*this, const_cast<NativeEvent*>(&event)))
    {
        if (!IsDialogMessageW(const_cast<MSG*>(&event)))
        {
            TranslateMessage(&event);
            DispatchMessageW(&event);
        }
    }

    return true;
}

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

void MainDialog::UpdateMRUList()
{
    CComboBox combo(GetDlgItem(IDC_SERVER_ADDRESS_COMBO));

    // Remove all items.
    combo.ResetContent();

    const int item_count = mru_.GetItemCount();

    if (!item_count)
    {
        UpdateCurrentComputer(MRU::GetDefaultConfig());
        return;
    }

    for (int index = item_count - 1; index >= 0; --index)
    {
        const proto::Computer& computer = mru_.GetItem(index);

        // Add address to combobox.
        const int item_index = combo.AddString(UNICODEfromUTF8(computer.address()).c_str());

        combo.SetItemData(item_index, index);

        if (index == item_count - 1)
        {
            UpdateCurrentComputer(computer);
            combo.SetCurSel(item_index);
        }
    }
}

void MainDialog::InitSessionTypesCombo()
{
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
}

void MainDialog::UpdateServerPort()
{
    CEdit port_edit = GetDlgItem(IDC_SERVER_PORT_EDIT);

    port_edit.SetLimitText(5);
    port_edit.SetWindowTextW(std::to_wstring(current_computer_.port()).c_str());

    if (current_computer_.port() == kDefaultHostTcpPort)
    {
        CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);
        port_edit.SetReadOnly(TRUE);
    }
    else
    {
        CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_UNCHECKED);
        port_edit.SetReadOnly(FALSE);
    }
}

LRESULT MainDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    accelerator_.LoadAcceleratorsW(IDC_MAIN_DIALOG_ACCELERATORS);

    const CSize small_icon_size(GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON));

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

    CMenuHandle lang_menu(main_menu_.GetSubMenu(2));
    lang_menu.CheckMenuRadioItem(ID_FIRST_LANGUAGE,
                                 ID_LAST_LANGUAGE,
                                 LangIdToCommandId(SettingsManager().GetUILanguage()),
                                 MF_BYCOMMAND);
    SetMenu(main_menu_);

    CString tray_tooltip;
    tray_tooltip.LoadStringW(IDS_APPLICATION_NAME);

    AddTrayIcon(small_icon_, tray_tooltip, IDR_TRAY);
    SetDefaultTrayMenuItem(ID_SHOWHIDE);

    InitIpList();
    UpdateIpList();
    InitSessionTypesCombo();
    UpdateMRUList();
    UpdateServerPort();

    if (HostService::IsInstalled())
        GetDlgItem(IDC_START_SERVER_BUTTON).EnableWindow(FALSE);

    const DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    const DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    GetDlgItem(IDC_SERVER_ADDRESS_COMBO).SetFocus();

    return FALSE;
}

LRESULT MainDialog::OnDefaultPortClicked(
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

LRESULT MainDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
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

LRESULT MainDialog::OnStartServerButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
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

LRESULT MainDialog::OnUpdateIpListButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    UpdateIpList();
    return 0;
}

void MainDialog::UpdateCurrentComputer(const proto::Computer& computer)
{
    current_computer_.CopyFrom(computer);

    CComboBox combo(GetDlgItem(IDC_SESSION_TYPE_COMBO));

    for (int i = combo.GetCount() - 1; i >= 0; --i)
    {
        if (static_cast<proto::auth::SessionType>(combo.GetItemData(i)) ==
            computer.session_type())
        {
            UpdateCurrentSessionType(computer.session_type());
            combo.SetCurSel(i);
            break;
        }
    }
}

void MainDialog::UpdateCurrentSessionType(proto::auth::SessionType session_type)
{
    current_computer_.set_session_type(session_type);

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

LRESULT MainDialog::OnSessionTypeChanged(
    WORD /* notify_code */, WORD /* control_id */, HWND control, BOOL& /* handled */)
{
    CComboBox combo(control);
    UpdateCurrentSessionType(static_cast<proto::auth::SessionType>(
        combo.GetItemData(combo.GetCurSel())));
    return 0;
}

LRESULT MainDialog::OnAddressChanged(
    WORD /* notify_code */, WORD /* control_id */, HWND control, BOOL& /* handled */)
{
    CComboBox combo(control);

    // Get current selected MRU index from combobox.
    const int item_index = static_cast<int>(combo.GetItemData(combo.GetCurSel()));

    // Validate MRU cache index.
    if (item_index >= 0 && item_index < mru_.GetItemCount())
    {
        // Address found in MRU list. Copy config from cache.
        current_computer_.CopyFrom(mru_.SetLastItem(item_index));
        UpdateMRUList();
        UpdateServerPort();
    }

    return 0;
}

LRESULT MainDialog::OnSettingsButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    switch (current_computer_.session_type())
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        {
            SettingsDialog dialog(current_computer_.session_type(),
                                  current_computer_.desktop_session());

            if (dialog.DoModal(*this) == IDOK)
            {
                current_computer_.mutable_desktop_session()->CopyFrom(dialog.Config());
            }
        }
        break;

        default:
            break;
    }

    return 0;
}

LRESULT MainDialog::OnConnectButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (current_computer_.session_type() != proto::auth::SESSION_TYPE_UNKNOWN)
    {
        // Update address and port in current config.
        current_computer_.set_address(
            UTF8fromUNICODE(GetWindowString(GetDlgItem(IDC_SERVER_ADDRESS_COMBO))));

        current_computer_.set_port(static_cast<uint32_t>(
            GetDlgItemInt(IDC_SERVER_PORT_EDIT, nullptr, FALSE)));

        // Add current config to MRU list.
        mru_.AddItem(current_computer_);

        // Reload MRU list to combobox.
        UpdateMRUList();

        if (!client_pool_)
            client_pool_ = std::make_unique<ClientPool>(MessageLoopProxy::Current());

        client_pool_->Connect(*this, current_computer_);
    }

    return 0;
}

LRESULT MainDialog::OnHelpButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CString url;
    url.LoadStringW(IDS_HELP_LINK);
    URL::FromCString(url).Open();
    return 0;
}

LRESULT MainDialog::OnShowHideButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    if (IsWindowVisible())
        ShowWindow(SW_HIDE);
    else
        ShowWindow(SW_SHOWNORMAL);

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

LRESULT MainDialog::OnCopyButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    CopySelectedIp();
    return 0;
}

LRESULT MainDialog::OnSelectLanguageButton(
    WORD /* notify_code */, WORD control_id, HWND /* control */, BOOL& /* handled */)
{
    LANGID langid = CommandIdToLangId(control_id);
    if (langid != 0)
    {
        CMenuHandle lang_menu(main_menu_.GetSubMenu(2));

        lang_menu.CheckMenuRadioItem(ID_FIRST_LANGUAGE,
                                     ID_LAST_LANGUAGE,
                                     control_id,
                                     MF_BYCOMMAND);

        SettingsManager().SetUILanguage(langid);

        CString title;
        title.LoadStringW(IDS_INFORMATION);

        CString message;
        message.LoadStringW(IDS_LANG_CHANGE_INFORMATION);

        MessageBoxW(message, title, MB_OK | MB_ICONINFORMATION);
    }

    return 0;
}

LRESULT MainDialog::OnIpListDoubleClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
    CopySelectedIp();
    return 0;
}

LRESULT MainDialog::OnIpListRightClick(
    int /* control_id */, LPNMHDR /* hdr */, BOOL& /* handled */)
{
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

LRESULT MainDialog::OnAddressBookButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    std::experimental::filesystem::path program_path;

    if (GetBasePath(aspia::BasePathKey::FILE_EXE, program_path))
    {
        CommandLine command_line(program_path);
        command_line.AppendSwitch(kAddressBookSwitch);
        LaunchProcess(command_line);
    }

    return 0;
}

LRESULT MainDialog::OnSystemInfoButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    LaunchSystemInfoProcess();
    return 0;
}

LRESULT MainDialog::OnExitButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    EndDialog(0);
    return 0;
}

LRESULT MainDialog::OnAboutButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    AboutDialog().DoModal(*this);
    return 0;
}

LRESULT MainDialog::OnUsersButton(
    WORD /* notify_code */, WORD /* control_id */, HWND /* control */, BOOL& /* handled */)
{
    UsersDialog().DoModal(*this);
    return 0;
}

} // namespace aspia
