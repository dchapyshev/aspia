/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main_dialog.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "gui/main_dialog.h"

#include <strsafe.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "base/exception.h"

namespace aspia {

static bool
AddressToString(const LPSOCKADDR src, WCHAR *dst, size_t size)
{
    if (src->sa_family == AF_INET)
    {
        struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in*>(src);

        static const WCHAR fmt[] = L"%u.%u.%u.%u";

        HRESULT hr = StringCbPrintfW(dst, size, fmt,
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

void CMainDialog::InitAddressesList()
{
    CListViewCtrl list(GetDlgItem(IDC_IP_LIST));
    list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT, 0);

    list.AddColumn(L"", 0);
    list.SetColumnWidth(0, 200);

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &size) != ERROR_BUFFER_OVERFLOW)
        return;

    PIP_ADAPTER_ADDRESSES adapter_addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(size));

    if (adapter_addresses)
    {
        if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapter_addresses, &size) == ERROR_SUCCESS)
        {
            for (PIP_ADAPTER_ADDRESSES adapter = adapter_addresses;
                 adapter != NULL;
                 adapter = adapter->Next)
            {
                if (adapter->OperStatus != IfOperStatusUp) continue;

                for (PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
                     address != NULL;
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

        free(adapter_addresses);
    }
}

LRESULT CMainDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    HICON small_icon = AtlLoadIconImage(IDI_MAINICON,
                                        LR_CREATEDIBSECTION,
                                        GetSystemMetrics(SM_CXSMICON),
                                        GetSystemMetrics(SM_CYSMICON));

    HICON big_icon = AtlLoadIconImage(IDI_MAINICON,
                                      LR_CREATEDIBSECTION,
                                      GetSystemMetrics(SM_CXICON),
                                      GetSystemMetrics(SM_CYICON));

    SetIcon(small_icon, FALSE);
    SetIcon(big_icon, TRUE);

    client_count_ = 0;
    UpdateConnectedClients();

    CenterWindow();

    InitAddressesList();

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, 11011, 0);
    SetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, L"192.168.89.70");

    CheckDlgButton(IDC_SERVER_DEFAULT_PORT, BST_CHECKED);

    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));
    port.SetReadOnly(TRUE);

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        // ѕереводим ввод в наше окно и делаем его активным.
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(*this);
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return 0;
}

LRESULT CMainDialog::OnDefaultPortClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));

    if (IsDlgButtonChecked(IDC_SERVER_DEFAULT_PORT) == BST_CHECKED)
    {
        port.SetReadOnly(TRUE);

        SetDlgItemInt(IDC_SERVER_PORT_EDIT, 11011, 0);
    }
    else
    {
        port.SetReadOnly(FALSE);
    }

    return 0;
}

LRESULT CMainDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&)
{
    DestroyIcon(GetIcon(FALSE));
    DestroyIcon(GetIcon(TRUE));

    EndDialog(0);
    return 0;
}

void CMainDialog::UpdateConnectedClients()
{
    CString msg;

    if (client_count_ > 0)
        msg.Format(IDS_CLIENT_COUNT, client_count_.load());
    else
        msg.LoadStringW(IDS_NO_CLIENTS);

    GetDlgItem(IDC_CLIENT_COUNT).SetWindowTextW(msg);
}

void CMainDialog::OnServerEvent(Host::Event type)
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

LRESULT CMainDialog::OnStartServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    LockGuard<Lock> guard(&server_lock_);

    CString status;
    CString button;

    try
    {
        if (!server_)
        {
            Host::OnEventCallback on_event_callback =
                std::bind(&CMainDialog::OnServerEvent, this, std::placeholders::_1);

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

    GetDlgItem(IDC_SERVER_STATUS).SetWindowTextW(status);
    GetDlgItem(IDC_START_SERVER).SetWindowTextW(button);

    return 0;
}

LRESULT CMainDialog::OnConnectToServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    try
    {
        WCHAR address[256] = { 0 };
        GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, address, ARRAYSIZE(address));

        std::unique_ptr<Socket> sock(new SocketTCP());

        sock->Connect(UTF8fromUNICODE(address), GetDlgItemInt(IDC_SERVER_PORT_EDIT));

        std::unique_ptr<ScreenWindow> window(new ScreenWindow(nullptr, std::move(sock)));

        thread_pool_.Insert(std::move(window));
    }
    catch (const Exception &err)
    {
        MessageBoxW(UNICODEfromUTF8(err.What()).c_str(),
                    nullptr,
                    MB_ICONWARNING | MB_OK);
    }

    return 0;
}

} // namespace aspia
