//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/main_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/main_dialog.h"

#include <atlmisc.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <uxtheme.h>

#include "base/version_helpers.h"
#include "base/process/process_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "client/client_config.h"
#include "host/host_service.h"
#include "ui/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/users_dialog.h"

namespace aspia {

static std::wstring
AddressToString(const LPSOCKADDR src)
{
    DCHECK_EQ(src->sa_family, AF_INET);

    sockaddr_in *addr = reinterpret_cast<sockaddr_in*>(src);

    return StringPrintfW(L"%u.%u.%u.%u",
                         addr->sin_addr.S_un.S_un_b.s_b1,
                         addr->sin_addr.S_un.S_un_b.s_b2,
                         addr->sin_addr.S_un.S_un_b.s_b3,
                         addr->sin_addr.S_un.S_un_b.s_b4);
}

void UiMainDialog::InitAddressesList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    DWORD ex_style = LVS_EX_FULLROWSELECT;

    if (IsWindowsVistaOrGreater())
    {
        SetWindowTheme(list, L"explorer", nullptr);
        ex_style |= LVS_EX_DOUBLEBUFFER;
    }

    list.SetExtendedListViewStyle(ex_style);

    int column_index = list.AddColumn(L"", 0);

    CRect list_rect;
    list.GetClientRect(list_rect);
    list.SetColumnWidth(column_index, list_rect.Width() - GetSystemMetrics(SM_CXVSCROLL));

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
    {
        LOG(ERROR) << "GetAdaptersAddresses() failed with unexpected error code";
        return;
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(size);

    PIP_ADAPTER_ADDRESSES addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.get());

    ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &size);
    if (result != ERROR_SUCCESS)
    {
        LOG(ERROR) << "GetAdaptersAddresses() failed: " << result;
        return;
    }

    for (PIP_ADAPTER_ADDRESSES adapter = addresses; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        for (PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
             address != nullptr;
             address = address->Next)
        {
            if (address->Address.lpSockaddr->sa_family != AF_INET)
                continue;

            std::wstring address_string = AddressToString(address->Address.lpSockaddr);

            if (address_string != L"127.0.0.1")
            {
                list.AddItem(list.GetItemCount(), 0, address_string.c_str(), 0);
            }
        }
    }
}

int UiMainDialog::AddSessionType(CComboBox& combobox,
                                 UINT string_resource_id,
                                 proto::SessionType session_type)
{
    CString text;
    text.LoadStringW(string_resource_id);

    int item_index = combobox.AddString(text);
    combobox.SetItemData(item_index, session_type);

    return item_index;
}

void UiMainDialog::InitSessionTypesCombo()
{
    CComboBox combobox(GetDlgItem(IDC_SESSION_TYPE_COMBO));

    int first_item = AddSessionType(combobox,
                                    IDS_SESSION_TYPE_DESKTOP_MANAGE,
                                    proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_DESKTOP_VIEW,
                   proto::SessionType::SESSION_TYPE_DESKTOP_VIEW);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_FILE_TRANSFER,
                   proto::SessionType::SESSION_TYPE_FILE_TRANSFER);

    AddSessionType(combobox,
                   IDS_SESSION_TYPE_POWER_MANAGE,
                   proto::SessionType::SESSION_TYPE_POWER_MANAGE);

    combobox.SetCurSel(first_item);
}

proto::SessionType UiMainDialog::GetSelectedSessionType()
{
    CComboBox combo(GetDlgItem(IDC_SESSION_TYPE_COMBO));
    return static_cast<proto::SessionType>(combo.GetItemData(combo.GetCurSel()));
}

LRESULT UiMainDialog::OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(small_icon_, TRUE);

    main_menu_ = AtlLoadMenu(IDR_MAIN);
    SetMenu(main_menu_);

    CString tray_tooltip;
    tray_tooltip.LoadStringW(IDS_APPLICATION_NAME);

    AddTrayIcon(small_icon_, tray_tooltip, IDR_TRAY);
    SetDefaultTrayMenuItem(ID_SHOWHIDE);

    InitAddressesList();
    InitSessionTypesCombo();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
    CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    CEdit(GetDlgItem(IDC_SERVER_PORT_EDIT)).SetReadOnly(TRUE);

    bool host_service_installed = HostService::IsInstalled();

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

    DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    GetDlgItem(IDC_SERVER_ADDRESS_EDIT).SetFocus();

    return FALSE;
}

LRESULT UiMainDialog::OnDefaultPortClicked(WORD notify_code,
                                           WORD control_id,
                                           HWND control,
                                           BOOL& handled)
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

LRESULT UiMainDialog::OnClose(UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL& handled)
{
    host_pool_.reset();
    client_pool_.reset();
    PostQuitMessage(0);
    return 0;
}

void UiMainDialog::StopHostMode()
{
    if (host_pool_)
    {
        CString text;
        text.LoadStringW(IDS_START);
        SetDlgItemTextW(IDC_START_SERVER_BUTTON, text);

        host_pool_.reset();
    }
}

LRESULT UiMainDialog::OnStartServerButton(WORD notify_code,
                                          WORD control_id,
                                          HWND control,
                                          BOOL& handled)
{
    if (host_pool_)
    {
        StopHostMode();
        return 0;
    }

    host_pool_ = std::make_unique<HostPool>(MessageLoopProxy::Current());

    if (host_pool_->Start())
    {
        CString text;
        text.LoadStringW(IDS_STOP);
        SetDlgItemTextW(IDC_START_SERVER_BUTTON, text);
        return 0;
    }

    host_pool_.reset();
    return 0;
}

void UiMainDialog::UpdateSessionType()
{
    proto::SessionType session_type = GetSelectedSessionType();

    switch (session_type)
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

LRESULT UiMainDialog::OnSessionTypeChanged(WORD notify_code,
                                           WORD control_id,
                                           HWND control,
                                           BOOL& handled)
{
    UpdateSessionType();
    return 0;
}

LRESULT UiMainDialog::OnSettingsButton(WORD notify_code,
                                       WORD control_id,
                                       HWND control,
                                       BOOL& handled)
{
    proto::SessionType session_type = GetSelectedSessionType();

    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
        {
            UiSettingsDialog dialog(session_type,
                                    config_.desktop_session_config());

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

LRESULT UiMainDialog::OnConnectButton(WORD notify_code,
                                      WORD control_id,
                                      HWND control,
                                      BOOL& handled)
{
    proto::SessionType session_type = GetSelectedSessionType();

    if (session_type != proto::SessionType::SESSION_TYPE_UNKNOWN)
    {
        WCHAR buffer[128] = { 0 };
        GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, buffer, _countof(buffer));

        config_.set_address(buffer);
        config_.set_port(GetDlgItemInt(IDC_SERVER_PORT_EDIT, nullptr, FALSE));
        config_.set_session_type(session_type);

        if (!client_pool_)
            client_pool_ = std::make_unique<ClientPool>(MessageLoopProxy::Current());

        client_pool_->Connect(*this, config_);
    }

    return 0;
}

LRESULT UiMainDialog::OnHelpButton(WORD notify_code,
                                   WORD control_id,
                                   HWND control,
                                   BOOL& handled)
{
    CString url;
    url.LoadStringW(IDS_HELP_LINK);
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT UiMainDialog::OnShowHideButton(WORD notify_code,
                                       WORD control_id,
                                       HWND control,
                                       BOOL& handled)
{
    if (IsWindowVisible())
        ShowWindow(SW_HIDE);
    else
        ShowWindow(SW_SHOWNORMAL);

    return 0;
}

LRESULT UiMainDialog::OnInstallServiceButton(WORD notify_code,
                                             WORD control_id,
                                             HWND control,
                                             BOOL& handled)
{
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

LRESULT UiMainDialog::OnRemoveServiceButton(WORD notify_code,
                                            WORD control_id,
                                            HWND control,
                                            BOOL& handled)
{
    if (HostService::Remove())
    {
        main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        GetDlgItem(IDC_START_SERVER_BUTTON).EnableWindow(TRUE);
    }

    return 0;
}

LRESULT UiMainDialog::OnExitButton(WORD notify_code,
                                   WORD control_id,
                                   HWND control,
                                   BOOL& handled)
{
    EndDialog(0);
    return 0;
}

LRESULT UiMainDialog::OnAboutButton(WORD notify_code,
                                    WORD control_id,
                                    HWND control,
                                    BOOL& handled)
{
    UiAboutDialog().DoModal(*this);
    return 0;
}

LRESULT UiMainDialog::OnUsersButton(WORD notify_code,
                                    WORD control_id,
                                    HWND control,
                                    BOOL& handled)
{
    UiUsersDialog().DoModal(*this);
    return 0;
}

} // namespace aspia
