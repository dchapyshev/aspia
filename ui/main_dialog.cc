//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/main_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/main_dialog.h"

#include <windowsx.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "base/version_helpers.h"
#include "base/elevation_helpers.h"
#include "base/process.h"
#include "base/unicode.h"
#include "base/util.h"
#include "client/client_config.h"
#include "codec/video_helpers.h"
#include "host/host_service.h"
#include "ui/viewer_window.h"
#include "ui/about_dialog.h"
#include "ui/users_dialog.h"
#include "ui/resource.h"

namespace aspia {

MainDialog::MainDialog()
{
    // Nothing
}

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
    HWND list = GetDlgItem(IDC_IP_LIST);

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
            {
                ListBox_AddString(list, address_string.c_str());
            }
        }
    }
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

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort);
    CheckDlgButton(hwnd(), IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    Edit_SetReadOnly(GetDlgItem(IDC_SERVER_PORT_EDIT), TRUE);

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(hwnd());
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    bool host_service_installed = HostService::IsInstalled();
    bool is_admin_mode = false;

    if (IsWindowsVistaOrGreater())
    {
        if (IsProcessElevated())
            is_admin_mode = true;
    }
    else
    {
        if (Process::Current().HasAdminRights())
            is_admin_mode = true;
    }

    if (!is_admin_mode)
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
        EnableWindow(GetDlgItem(IDC_START_SERVER_BUTTON), FALSE);
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
    EndDialog(0);
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

static void SetDefaultDesktopSessionConfig(proto::DesktopSessionConfig* config)
{
    config->set_flags(proto::DesktopSessionConfig::ENABLE_CLIPBOARD |
                     proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE);

    config->set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_ZLIB);

    config->set_update_interval(30);
    config->set_compress_ratio(6);

    ConvertToVideoPixelFormat(PixelFormat::RGB565(), config->mutable_pixel_format());
}

void MainDialog::OnConnectButton()
{
    ClientConfig config;

    config.SetRemoteAddress(GetDlgItemString(IDC_SERVER_ADDRESS_EDIT));
    config.SetRemotePort(GetDlgItemInt<uint16_t>(IDC_SERVER_PORT_EDIT));
    config.SetSessionType(proto::SessionType::SESSION_DESKTOP_MANAGE);

    SetDefaultDesktopSessionConfig(config.mutable_desktop_session_config());

    if (!client_pool_)
        client_pool_.reset(new ClientPool(MessageLoopProxy::Current()));

    client_pool_->Connect(hwnd(), config);
}

void MainDialog::OnExitButton()
{
    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
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

void MainDialog::OnAboutButton()
{
    AboutDialog dialog;
    dialog.DoModal(hwnd());
}

void MainDialog::OnUsersButton()
{
    UsersDialog dialog;
    dialog.DoModal(hwnd());
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
        EnableWindow(GetDlgItem(IDC_START_SERVER_BUTTON), FALSE);
    }
}

void MainDialog::OnRemoveServiceButton()
{
    if (HostService::Remove())
    {
        EnableMenuItem(main_menu_, ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(main_menu_, ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_GRAYED);
        EnableWindow(GetDlgItem(IDC_START_SERVER_BUTTON), TRUE);
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

                case IDC_CONNECT_BUTTON:
                    OnConnectButton();
                    break;

                case IDC_SERVER_DEFAULT_PORT_CHECK:
                    OnDefaultPortClicked();
                    break;

                case ID_EXIT:
                    OnExitButton();
                    break;

                case ID_HELP:
                    OnHelpButton();
                    break;

                case ID_ABOUT:
                    OnAboutButton();
                    break;

                case ID_USERS:
                    OnUsersButton();
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
