/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main_dialog.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_GUI__MAIN_DIALOG_H
#define _ASPIA_GUI__MAIN_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlmisc.h>

#include <atomic>

#include "gui/resource.h"

#include "base/thread_pool.h"
#include "client/screen_window.h"
#include "network/server_tcp.h"
#include "host/host.h"
#include "client/client.h"

namespace aspia {

class CMainDialog : public CDialogImpl < CMainDialog >
{
public:
    enum { IDD = IDD_MAINDIALOG };

    BEGIN_MSG_MAP(CMainDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_START_SERVER, OnStartServer)
        COMMAND_ID_HANDLER(IDC_CONNECT, OnConnectToServer)

        COMMAND_ID_HANDLER(IDC_SERVER_DEFAULT_PORT, OnDefaultPortClicked)
    END_MSG_MAP()

private:
    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
    LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

    LRESULT OnDefaultPortClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
    LRESULT OnStartServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
    LRESULT OnConnectToServer(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

    void InitAddressesList();
    void UpdateConnectedClients();

    void OnServerEvent(Host::Event type);

private:
    std::unique_ptr<ServerTCP<Host>> server_;
    Lock server_lock_;
    std::atomic_int client_count_;

    ThreadPool thread_pool_;
};

} // namespace aspia

#endif // _ASPIA_GUI__MAIN_DIALOG_H
