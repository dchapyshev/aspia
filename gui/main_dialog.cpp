/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main_dialog.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "gui/main_dialog.h"

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

    CenterWindow();

    CListViewCtrl list(GetDlgItem(IDC_CLIENT_LIST));
    list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT, 0);

    list.AddColumn(L"ID", 0);
    list.SetColumnWidth(0, 50);

    list.AddColumn(L"Address", 1);
    list.SetColumnWidth(1, 150);

    SetDlgItemInt(IDC_SERVER_PORT_EDIT, 11011, 0);
    SetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, L"192.168.89.70");

    CheckDlgButton(IDC_SERVER_DEFAULT_PORT, BST_CHECKED);

    CEdit port(GetDlgItem(IDC_SERVER_PORT_EDIT));
    port.SetReadOnly(TRUE);

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        // Переводим ввод в наше окно и делаем его активным.
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

void CMainDialog::OnClientConnected(uint32_t client_id, const std::string &address)
{
    CListViewCtrl list(GetDlgItem(IDC_CLIENT_LIST));

    int index = list.AddItem(list.GetItemCount(), 0, std::to_wstring(client_id).c_str());

    list.AddItem(index, 1, UNICODEfromUTF8(address).c_str());
    list.SetItemData(index, client_id);
}

void CMainDialog::OnClientRejected(const std::string &address)
{
    std::wstring msg(L"Detected rejected the connection attempt from address: ");

    msg += UNICODEfromUTF8(address);

    MessageBox(msg.c_str(), nullptr, MB_OK | MB_ICONWARNING);
}

void CMainDialog::OnClientDisconnected(uint32_t client_id)
{
    CListViewCtrl list(GetDlgItem(IDC_CLIENT_LIST));

    int count = list.GetItemCount();

    for (int i = 0; i < count; ++i)
    {
        if (list.GetItemData(i) == client_id)
        {
            list.DeleteItem(i);
            break;
        }
    }
}

LRESULT CMainDialog::OnStartServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    LockGuard<Mutex> guard(&server_lock_);

    CString status;
    CString button;

    try
    {
        if (!server_)
        {
            // Создаем сервер.
            std::unique_ptr<Server> server(new Server("", 11011));

            Server::OnClientConnectedCallback on_connected =
                std::bind(&CMainDialog::OnClientConnected, this, std::placeholders::_1, std::placeholders::_2);

            Server::OnClientRejectedCallback on_rejected =
                std::bind(&CMainDialog::OnClientRejected, this, std::placeholders::_1);

            Server::OnClientDisconnectedCallback on_disconnected =
                std::bind(&CMainDialog::OnClientDisconnected, this, std::placeholders::_1);

            // Регистрируем callback функции для получения уведомлений от сервера.
            server->RegisterCallbacks(on_connected, on_rejected, on_disconnected);

            // Запускаем поток сервера.
            server->Start();

            status.LoadStringW(IDS_SERVER_STARTED);
            button.LoadStringW(IDS_STOP);

            server_ = std::move(server);
        }
        else
        {
            // Даем команду остановить поток сервера.
            server_->Stop();

            // Дожидаемся завершения работы потока сервера.
            server_->WaitForEnd();

            // Уничтожаем экземпляр класса сервера.
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

void CMainDialog::OnRemoteClientEvent(RemoteClient::EventType type)
{
    if (type == RemoteClient::EventType::NotConnected)
    {
        MessageBoxW(L"Unable to connect.", L"Error", MB_OK | MB_ICONWARNING);
        GetDlgItem(IDC_CONNECT).EnableWindow(TRUE);
        client_.reset();
    }
    else if (type == RemoteClient::EventType::Disconnected)
    {
        GetDlgItem(IDC_CONNECT).EnableWindow(TRUE);
        client_.reset();
    }
    else if (type == RemoteClient::EventType::BadAuth)
    {
        MessageBoxW(L"BadAuth", L"Error", MB_OK | MB_ICONWARNING);
        GetDlgItem(IDC_CONNECT).EnableWindow(TRUE);
        client_.reset();
    }
    else if (type == RemoteClient::EventType::Connected)
    {

    }
}

LRESULT CMainDialog::OnConnectToServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    try
    {
        GetDlgItem(IDC_CONNECT).EnableWindow(FALSE);

        WCHAR addr[256] = { 0 };

        GetDlgItemTextW(IDC_SERVER_ADDRESS_EDIT, addr, ARRAYSIZE(addr));

        std::string address = UTF8fromUNICODE(addr);

        if (!Socket::IsValidHostName(address.c_str()))
        {
            CEdit edit(GetDlgItem(IDC_SERVER_ADDRESS_EDIT));

            edit.SetSel(0, edit.LineLength());
            edit.SetFocus();

            throw Exception("Wrong server address.");
        }

        int port = GetDlgItemInt(IDC_SERVER_PORT_EDIT);

        if (!Socket::IsValidPort(port))
        {
            CEdit edit(GetDlgItem(IDC_SERVER_PORT_EDIT));

            edit.SetSel(0, edit.LineLength());
            edit.SetFocus();

            throw Exception("Wrong server port.");
        }

        RemoteClient::OnEventCallback on_event =
            std::bind(&CMainDialog::OnRemoteClientEvent, this, std::placeholders::_1);

        // Создаем клиент.
        client_.reset(new RemoteClient(on_event));

        // Подключаемся к указанному серверу.
        client_->ConnectTo(address.c_str(), port);
    }
    catch (const Exception &err)
    {
        MessageBoxW(UNICODEfromUTF8(err.What()).c_str(),
                    nullptr,
                    MB_ICONWARNING | MB_OK);
    }

    return 0;
}
