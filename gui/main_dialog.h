//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/main_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__MAIN_DIALOG_H
#define _ASPIA_GUI__MAIN_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include <atomic>

#include "gui/resource.h"

#include "base/thread_pool.h"
#include "gui/status_dialog.h"
#include "gui/tray_icon.h"
#include "network/server_tcp.h"
#include "host/host.h"

namespace aspia {

class MainDialog : public CDialogImpl<MainDialog>, public TrayIcon<MainDialog>
{
public:
    MainDialog();
    ~MainDialog() = default;

    enum { IDD = IDD_MAIN };

private:
    BEGIN_MSG_MAP(MainDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)

        COMMAND_ID_HANDLER(IDC_START_SERVER_BUTTON, OnStartServer)
        COMMAND_ID_HANDLER(IDC_CONNECT_BUTTON, OnConnectButton)

        COMMAND_ID_HANDLER(IDC_SERVER_DEFAULT_PORT_CHECK, OnDefaultPortClicked)

        COMMAND_ID_HANDLER(ID_APP_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_HELP, OnHelpButton)
        COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_USERS, OnUsersButton)
        COMMAND_ID_HANDLER(ID_SHOWHIDE, OnShowHideButton)
        COMMAND_ID_HANDLER(ID_INSTALL_SERVICE, OnInstallServiceButton)
        COMMAND_ID_HANDLER(ID_REMOVE_SERVICE, OnRemoveServiceButton)

        CHAIN_MSG_MAP(TrayIcon<MainDialog>)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();

    LRESULT OnDefaultPortClicked(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnStartServer(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);
    LRESULT OnConnectButton(WORD notify_code, WORD id, HWND hWndCtl, BOOL& handled);

    LRESULT OnExitButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnHelpButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnAboutButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnUsersButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnShowHideButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnInstallServiceButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnRemoveServiceButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);

    void InitAddressesList();
    void UpdateConnectedClients();

    void OnServerEvent(Host::SessionEvent event);

private:
    CMenu main_menu_;

    bool is_hidden_;

    std::unique_ptr<ServerTCP<Host>> host_server_;
    Lock server_lock_;
    std::atomic_int client_count_;

    ThreadPool thread_pool_;
};

} // namespace aspia

#endif // _ASPIA_GUI__MAIN_DIALOG_H
