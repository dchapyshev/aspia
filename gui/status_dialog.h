//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/status_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__STATUS_DIALOG_H
#define _ASPIA_GUI__STATUS_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlcrack.h>

#include "base/thread.h"

#include "gui/viewer_window.h"
#include "gui/window_thread.h"
#include "gui/resource.h"

#include "network/client_tcp.h"
#include "client/client.h"

namespace aspia {

class StatusDialog :
    public CDialogImpl<StatusDialog>,
    public Thread
{
public:
    enum { IDD = IDD_STATUS };

    BEGIN_MSG_MAP(StatusDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)
        MESSAGE_HANDLER(WM_APP, OnAppMessage);

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    StatusDialog(const ClientConfig &config) :
        config_(config)
    {
        Start();
    }

    ~StatusDialog()
    {
        if (IsActiveThread())
        {
            Stop();
            WaitForEnd();
        }
    }

    void OnClientEvent(Client::Event type);

private:
    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
    LRESULT OnAppMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled);
    LRESULT OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);

    void OnConnected(std::unique_ptr<Socket> &sock);
    void OnError(const char *reason);

    void AddMessage(const WCHAR *message);
    void AddMessage(UINT message_id);

    void Worker() override
    {
        DoModal(nullptr);
    }

    void OnStop() override
    {
        PostMessageW(WM_CLOSE);
    }

private:
    ClientConfig config_;

    std::unique_ptr<ClientTCP> tcp_;
    std::unique_ptr<Client> client_;

    std::unique_ptr<ViewerWindow> viewer_;
};

} // namespace aspia

#endif // _ASPIA_GUI__STATUS_DIALOG_H
