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

#include "gui/dialog_thread.h"
#include "gui/viewer_window.h"
#include "gui/resource.h"

#include "network/client_tcp.h"
#include "client/client_session_desktop.h"

namespace aspia {

class StatusDialog :
    public CDialogImpl<StatusDialog>,
    private ClientTCP
{
public:
    enum { IDD = IDD_STATUS };

    StatusDialog(const ClientConfig& config);
    ~StatusDialog() = default;

private:
    BEGIN_MSG_MAP(StatusDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)
        MESSAGE_HANDLER(WM_APP, OnAppMessage);

        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
    LRESULT OnAppMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);

    void OnConnectionSuccess(std::unique_ptr<Socket> sock) override;
    void OnConnectionError() override;

    void AddMessage(const WCHAR* message);
    void AddMessage(UINT message_id);

private:
    ClientConfig config_;

    std::unique_ptr<DialogThread<ViewerWindow>> viewer_window_;
};

} // namespace aspia

#endif // _ASPIA_GUI__STATUS_DIALOG_H
