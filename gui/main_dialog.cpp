//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/main_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/main_dialog.h"

#include <strsafe.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "base/exception.h"
#include "base/scoped_native_library.h"
#include "gui/viewer_window.h"
#include "client/client_config.h"

namespace aspia {

extern CIcon _small_icon;
extern CIcon _big_icon;

static bool
AddressToString(const LPSOCKADDR src, WCHAR *dst, size_t size)
{
    if (src->sa_family == AF_INET)
    {
        struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in*>(src);

        HRESULT hr = StringCbPrintfW(dst, size, L"%u.%u.%u.%u",
                                     addr->sin_addr.S_un.S_un_b.s_b1,
                                     addr->sin_addr.S_un.S_un_b.s_b2,
                                     addr->sin_addr.S_un.S_un_b.s_b3,
                                     addr->sin_addr.S_un.S_un_b.s_b4);
        if (SUCCEEDED(hr))
        {
            return true;
        }
    }

    return false;
}

void MainDialog::InitAddressesList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));

    ScopedNativeLibrary uxtheme("uxtheme.dll");

    typedef LRESULT(WINAPI *SETWINDOWTHEME)(HWND, LPCWSTR, LPCWSTR);

    SETWINDOWTHEME set_window_theme_func =
        reinterpret_cast<SETWINDOWTHEME>(uxtheme.GetFunctionPointer("SetWindowTheme"));

    if (set_window_theme_func)
        set_window_theme_func(list, L"Explorer", 0);

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
                    WCHAR buffer[256];

                    if (AddressToString(address->Address.lpSockaddr, buffer, sizeof(buffer)) &&
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

    client_count_ = 0;
    UpdateConnectedClients();

    CenterWindow();

    CString title;
    title.LoadStringW(IDS_APPLICATION_NAME);
    AddTrayIcon(title, _small_icon, IDR_TRAY, ID_SHOWHIDE);

    InitAddressesList();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, 11011, 0);
    SetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, L"192.168.89.61");

    CheckDlgButton(IDC_SERVER_DEFAULT_PORT_CHECK, BST_CHECKED);

    CEdit(GetDlgItem(IDC_SERVER_PORT_EDIT)).SetReadOnly(TRUE);

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        // ѕереводим ввод в наше окно и делаем его активным.
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return TRUE;
}

LRESULT MainDialog::OnDefaultPortClicked(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));

    if (IsDlgButtonChecked(IDC_SERVER_DEFAULT_PORT_CHECK) == BST_CHECKED)
    {
        SetDlgItemInt(IDC_SERVER_PORT_EDIT, 11011, 0);
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

void MainDialog::OnServerEvent(Host::Event type)
{
    if (type == Host::Event::Connected)
    {
        ++client_count_;
    }
    else if (type == Host::Event::Disconnected)
    {
        --client_count_;
    }

    UpdateConnectedClients();
}

LRESULT MainDialog::OnStartServer(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    LockGuard<Lock> guard(&server_lock_);

    CString status;
    CString button;

    try
    {
        if (!server_)
        {
            Host::OnEventCallback on_event_callback =
                std::bind(&MainDialog::OnServerEvent, this, std::placeholders::_1);

            server_.reset(new ServerTCP<Host>(11011, on_event_callback));

            status.LoadStringW(IDS_SERVER_STARTED);
            button.LoadStringW(IDS_STOP);
        }
        else
        {
            client_count_ = 0;
            UpdateConnectedClients();

            // ”ничтожаем экземпл€р класса сервера.
            server_.reset();

            status.LoadStringW(IDS_SERVER_STOPPED);
            button.LoadStringW(IDS_START);
        }
    }
    catch (const Exception &err)
    {
        MessageBoxW(UNICODEfromUTF8(err.What()).c_str(),
                    nullptr,
                    MB_ICONWARNING | MB_OK);

        status.LoadStringW(IDS_SERVER_STOPPED);
        button.LoadStringW(IDS_START);
    }

    SetDlgItemTextW(IDC_SERVER_STATUS_TEXT, status);
    SetDlgItemTextW(IDC_START_SERVER_BUTTON, button);

    return 0;
}

LRESULT MainDialog::OnConnectButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    WCHAR address[256];
    GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, address, ARRAYSIZE(address));

    int port = GetDlgItemInt(IDC_SERVER_PORT_EDIT);

    ClientConfig config;

    config.SetRemoteAddress(UTF8fromUNICODE(address));
    config.SetRemotePort(port);

    std::unique_ptr<StatusDialog> dialog(new StatusDialog(config));

    thread_pool_.Insert(std::move(dialog));

    return 0;
}

LRESULT MainDialog::OnExitButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT MainDialog::OnShowHideButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    ShowWindow(is_hidden_ ? SW_SHOW : SW_HIDE);
    is_hidden_ = !is_hidden_;

    return 0;
}

} // namespace aspia
