//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/main_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/main_dialog.h"

#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "base/version_helpers.h"
#include "base/process_helpers.h"
#include "base/unicode.h"
#include "base/string_util.h"
#include "client/client_config.h"
#include "host/host_service.h"
#include "ui/base/listview.h"
#include "ui/base/combobox.h"
#include "ui/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/users_dialog.h"
#include "ui/resource.h"

namespace aspia {

INT_PTR MainDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_MAIN);
}

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

void MainDialog::InitAddressesList()
{
    ListView list(GetDlgItem(IDC_IP_LIST));

    list.ModifyExtendedListViewStyle(0, LVS_EX_FULLROWSELECT);
    list.AddOnlyOneColumn();

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
    {
        LOG(ERROR) << "GetAdaptersAddresses() failed with unexpected error code";
        return;
    }

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);

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
                list.AddItem(address_string, 0);
        }
    }
}

void MainDialog::InitSessionTypesCombo()
{
    HWND combo = GetDlgItem(IDC_SESSION_TYPE_COMBO);

    ComboBox_AddItem(combo,
                     module().string(IDS_SESSION_TYPE_DESKTOP_MANAGE),
                     proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE);

    ComboBox_AddItem(combo,
                     module().string(IDS_SESSION_TYPE_DESKTOP_VIEW),
                     proto::SessionType::SESSION_TYPE_DESKTOP_VIEW);

    ComboBox_AddItem(combo,
                     module().string(IDS_SESSION_TYPE_FILE_TRANSFER),
                     proto::SessionType::SESSION_TYPE_FILE_TRANSFER);

    ComboBox_AddItem(combo,
                     module().string(IDS_SESSION_TYPE_POWER_MANAGE),
                     proto::SessionType::SESSION_TYPE_POWER_MANAGE);

    ComboBox_SelItemWithData(combo, proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE);
}

proto::SessionType MainDialog::GetSelectedSessionType()
{
    HWND combo = GetDlgItem(IDC_SESSION_TYPE_COMBO);
    return static_cast<proto::SessionType>(ComboBox_CurItemData(combo));
}

void MainDialog::OnInitDialog()
{
    SetIcon(IDI_MAIN);

    main_menu_.Reset(module().menu(IDR_MAIN));
    SetMenu(hwnd(), main_menu_);

    tray_icon_.AddIcon(hwnd(),
                       IDI_MAIN,
                       module().string(IDS_APPLICATION_NAME),
                       IDR_TRAY);
    tray_icon_.SetDefaultMenuItem(ID_SHOWHIDE);

    InitAddressesList();
    InitSessionTypesCombo();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
    CheckDlgButton(hwnd(), IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    Edit_SetReadOnly(GetDlgItem(IDC_SERVER_PORT_EDIT), TRUE);

    bool host_service_installed = HostService::IsInstalled();

    if (!IsCallerHasAdminRights())
    {
        EnableMenuItem(main_menu_, ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(main_menu_, ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
    }
    else
    {
        if (host_service_installed)
            EnableMenuItem(main_menu_, ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        else
            EnableMenuItem(main_menu_, ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
    }

    if (host_service_installed)
        EnableDlgItem(IDC_START_SERVER_BUTTON, false);

    OnSessionTypeChanged();
    SetForegroundWindowEx();
}

void MainDialog::OnDefaultPortClicked()
{
    HWND port = GetDlgItem(IDC_SERVER_PORT_EDIT);

    if (IsDlgButtonChecked(hwnd(), IDC_SERVER_DEFAULT_PORT_CHECK) == BST_CHECKED)
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
        Edit_SetReadOnly(port, TRUE);
    }
    else
    {
        Edit_SetReadOnly(port, FALSE);
    }
}

void MainDialog::OnClose()
{
    host_pool_.reset();
    client_pool_.reset();
    EndDialog();
}

void MainDialog::StopHostMode()
{
    if (host_pool_)
    {
        SetDlgItemString(IDC_START_SERVER_BUTTON, IDS_START);
        host_pool_.reset();
    }
}

void MainDialog::OnStartServerButton()
{
    if (host_pool_)
    {
        StopHostMode();
        return;
    }

    host_pool_.reset(new HostPool());

    if (host_pool_->Start())
    {
        SetDlgItemString(IDC_START_SERVER_BUTTON, IDS_STOP);
        return;
    }

    host_pool_.reset();
}

void MainDialog::OnSessionTypeChanged()
{
    proto::SessionType session_type = GetSelectedSessionType();

    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
        {
            config_.SetDefaultDesktopSessionConfig();
            EnableDlgItem(IDC_SETTINGS_BUTTON, true);
        }
        break;

        default:
            EnableDlgItem(IDC_SETTINGS_BUTTON, false);
            break;
    }
}

void MainDialog::OnSettingsButton()
{
    proto::SessionType session_type = GetSelectedSessionType();

    switch (session_type)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
        {
            SettingsDialog dialog;

            if (dialog.DoModal(hwnd(),
                               session_type,
                               config_.desktop_session_config()) == IDOK)
            {
                config_.mutable_desktop_session_config()->CopyFrom(dialog.Config());
            }
        }
        break;

        default:
            break;
    }
}

void MainDialog::OnConnectButton()
{
    proto::SessionType session_type = GetSelectedSessionType();

    if (session_type != proto::SessionType::SESSION_TYPE_UNKNOWN)
    {
        config_.set_address(GetDlgItemString(IDC_SERVER_ADDRESS_EDIT));
        config_.set_port(GetDlgItemInt<uint16_t>(IDC_SERVER_PORT_EDIT));
        config_.set_session_type(session_type);

        if (!client_pool_)
            client_pool_.reset(new ClientPool(MessageLoopProxy::Current()));

        client_pool_->Connect(hwnd(), config_);
    }
}

void MainDialog::OnHelpButton()
{
    ShellExecuteW(nullptr,
                  L"open",
                  module().string(IDS_HELP_LINK).c_str(),
                  nullptr,
                  nullptr,
                  SW_SHOWNORMAL);
}

void MainDialog::OnShowHideButton()
{
    if (IsWindowVisible(hwnd()))
        ShowWindow(hwnd(), SW_HIDE);
    else
        ShowWindow(hwnd(), SW_SHOWNORMAL);
}

void MainDialog::OnInstallServiceButton()
{
    if (host_pool_)
    {
        StopHostMode();
    }

    if (HostService::Install())
    {
        EnableMenuItem(main_menu_, ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(main_menu_, ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        EnableDlgItem(IDC_START_SERVER_BUTTON, false);
    }
}

void MainDialog::OnRemoveServiceButton()
{
    if (HostService::Remove())
    {
        EnableMenuItem(main_menu_, ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(main_menu_, ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        EnableDlgItem(IDC_START_SERVER_BUTTON, true);
    }
}

INT_PTR MainDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDC_START_SERVER_BUTTON:
                    OnStartServerButton();
                    break;

                case IDC_SETTINGS_BUTTON:
                    OnSettingsButton();
                    break;

                case IDC_CONNECT_BUTTON:
                    OnConnectButton();
                    break;

                case IDC_SERVER_DEFAULT_PORT_CHECK:
                    OnDefaultPortClicked();
                    break;

                case ID_EXIT:
                    EndDialog();
                    break;

                case ID_HELP:
                    OnHelpButton();
                    break;

                case ID_ABOUT:
                    AboutDialog().DoModal(hwnd());
                    break;

                case ID_USERS:
                    UsersDialog().DoModal(hwnd());
                    break;

                case ID_SHOWHIDE:
                    OnShowHideButton();
                    break;

                case ID_INSTALL_SERVICE:
                    OnInstallServiceButton();
                    break;

                case ID_REMOVE_SERVICE:
                    OnRemoveServiceButton();
                    break;

                case IDC_SESSION_TYPE_COMBO:
                {
                    if (HIWORD(wparam) == CBN_SELCHANGE)
                        OnSessionTypeChanged();
                }
                break;
            }
        }
        break;

        case WM_CLOSE:
            OnClose();
            break;

        default:
            tray_icon_.HandleMessage(msg, wparam, lparam);
            break;
    }

    return 0;
}

} // namespace aspia
