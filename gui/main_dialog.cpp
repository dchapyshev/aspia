//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/main_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/main_dialog.h"

#include <iphlpapi.h>
#include <uxtheme.h>
#include <ws2tcpip.h>

#include "base/process.h"
#include "base/unicode.h"
#include "gui/viewer_window.h"
#include "gui/about_dialog.h"
#include "gui/users_dialog.h"
#include "client/client_config.h"

namespace aspia {

extern CIcon _small_icon;
extern CIcon _big_icon;

MainDialog::MainDialog() :
    is_hidden_(false),
    main_menu_(AtlLoadMenu(IDR_MAIN))
{
    client_count_ = 0;
}

void MainDialog::InitAddressesList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT, 0);

    list.AddColumn(L"", 0);
    list.SetColumnWidth(0, 195);

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW)
        return;

    PIP_ADAPTER_ADDRESSES addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(size));

    if (addresses)
    {
        if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addresses, &size) == ERROR_SUCCESS)
        {
            for (PIP_ADAPTER_ADDRESSES adapter = addresses; adapter; adapter = adapter->Next)
            {
                if (adapter->OperStatus != IfOperStatusUp) continue;

                for (PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
                     address != nullptr;
                     address = address->Next)
                {
                    if (address->Address.lpSockaddr->sa_family != AF_INET)
                        continue;

                    sockaddr_in *addr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                    WCHAR buffer[128] = { 0 };

                    if (InetNtopW(AF_INET, &(addr->sin_addr), buffer, ARRAYSIZE(buffer)) &&
                        wcscmp(buffer, L"127.0.0.1") != 0)
                    {
                        list.AddItem(list.GetItemCount(), 0, buffer);
                    }
                }
            }
        }

        free(addresses);
    }
}

BOOL MainDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    SetIcon(_small_icon, FALSE);
    SetIcon(_big_icon, TRUE);
    SetMenu(main_menu_);

    client_count_ = 0;
    UpdateConnectedClients();

    CenterWindow();

    CString title;
    title.LoadStringW(IDS_APPLICATION_NAME);
    AddTrayIcon(title, _small_icon, IDR_TRAY, ID_SHOWHIDE);

    InitAddressesList();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort, 0);
    SetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, L"192.168.89.62");

    CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    CEdit(GetDlgItem(IDC_SERVER_PORT_EDIT)).SetReadOnly(TRUE);

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        // Переводим ввод в наше окно и делаем его активным.
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    if (!Process::Current().IsElevated())
    {
        main_menu_.EnableMenuItem(ID_INSTALL_SERVICE, MF_BYCOMMAND | MF_DISABLED);
        main_menu_.EnableMenuItem(ID_REMOVE_SERVICE, MF_BYCOMMAND | MF_DISABLED);
    }

    return TRUE;
}

LRESULT MainDialog::OnDefaultPortClicked(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));

    if (IsDlgButtonChecked(IDC_SERVER_DEFAULT_PORT_CHECK) == BST_CHECKED)
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, kDefaultHostTcpPort, 0);
        port.SetReadOnly(TRUE);
    }
    else
    {
        port.SetReadOnly(FALSE);
    }

    return 0;
}

void MainDialog::OnClose()
{
    thread_pool_.Clear();
    EndDialog(0);
}

void MainDialog::UpdateConnectedClients()
{
    CString msg;

    if (client_count_ > 0)
        msg.Format(IDS_CLIENT_COUNT_FORMAT, client_count_.load());
    else
        msg.LoadStringW(IDS_NO_CLIENTS);

    SetDlgItemTextW(IDC_CLIENT_COUNT_TEXT, msg);
}

void MainDialog::OnServerEvent(Host::SessionEvent event)
{
    if (event == Host::SessionEvent::OPEN)
    {
        ++client_count_;
    }
    else if (event == Host::SessionEvent::CLOSE)
    {
        --client_count_;
    }

    UpdateConnectedClients();
}

LRESULT MainDialog::OnStartServer(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    AutoLock lock(server_lock_);

    CString status;
    CString button;

    if (!host_server_)
    {
        Host::EventCallback event_callback =
            std::bind(&MainDialog::OnServerEvent, this, std::placeholders::_1);

        host_server_.reset(new ServerTCP<Host>(kDefaultHostTcpPort, event_callback));

        status.LoadStringW(IDS_SERVER_STARTED);
        button.LoadStringW(IDS_STOP);
    }
    else
    {
        client_count_ = 0;
        UpdateConnectedClients();

        // Уничтожаем экземпляр класса сервера.
        host_server_.reset();

        status.LoadStringW(IDS_SERVER_STOPPED);
        button.LoadStringW(IDS_START);
    }

    SetDlgItemTextW(IDC_SERVER_STATUS_TEXT, status);
    SetDlgItemTextW(IDC_START_SERVER_BUTTON, button);

    return 0;
}

LRESULT MainDialog::OnConnectButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    WCHAR address[128];

    if (GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, address, ARRAYSIZE(address)))
    {
        BOOL success = FALSE;
        uint16_t port = GetDlgItemInt(IDC_SERVER_PORT_EDIT, &success, FALSE);

        if (success)
        {
            ClientConfig config;

            config.SetRemoteAddress(UTF8fromUNICODE(address));
            config.SetRemotePort(port);

            // Создаем экземпляр диалога.
            std::unique_ptr<StatusDialog> dialog(new StatusDialog(config));

            // Создаем поток, в котором будет выполняться диалог.
            std::unique_ptr<DialogThread<StatusDialog>> thread(new DialogThread<StatusDialog>(std::move(dialog)));

            // Помещаем поток в пул.
            thread_pool_.Insert(std::move(thread));
        }
    }

    return 0;
}

LRESULT MainDialog::OnExitButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT MainDialog::OnHelpButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    CString link;
    link.LoadStringW(IDS_HELP_LINK);
    ShellExecuteW(nullptr, L"open", link, nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

LRESULT MainDialog::OnAboutButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    AboutDialog dialog;
    dialog.DoModal();
    return 0;
}

LRESULT MainDialog::OnUsersButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    UsersDialog dialog;
    dialog.DoModal();
    return 0;
}

LRESULT MainDialog::OnShowHideButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    ShowWindow(is_hidden_ ? SW_SHOW : SW_HIDE);
    is_hidden_ = !is_hidden_;
    return 0;
}

LRESULT MainDialog::OnInstallServiceButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    return 0;
}

LRESULT MainDialog::OnRemoveServiceButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled)
{
    return 0;
}

} // namespace aspia
