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

#include "gui/resource.h"

#include "server/server.h"
#include "client/remote_client.h"

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

    void OnClientConnected(uint32_t client_id, const std::string &address);
    void OnClientRejected(const std::string &address);
    void OnClientDisconnected(uint32_t client_id);

    void OnRemoteClientEvent(RemoteClient::EventType type);

private:
    std::unique_ptr<Server> server_;
    Mutex server_lock_;

    std::unique_ptr<RemoteClient> client_;
};

#endif // _ASPIA_GUI__MAIN_DIALOG_H
