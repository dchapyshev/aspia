//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/main_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__MAIN_DIALOG_H
#define _ASPIA_UI__MAIN_DIALOG_H

#include "host/host_pool.h"
#include "client/client_pool.h"
#include "ui/base/tray_icon.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class UiMainDialog :
    public CDialogImpl<UiMainDialog>,
    public CTrayIcon<UiMainDialog>
{
public:
    enum { IDD = IDD_MAIN };

    UiMainDialog() = default;
    ~UiMainDialog() = default;

private:
    BEGIN_MSG_MAP(UiMainDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)

        COMMAND_ID_HANDLER(IDC_START_SERVER_BUTTON, OnStartServerButton)
        COMMAND_ID_HANDLER(IDC_SETTINGS_BUTTON, OnSettingsButton)
        COMMAND_ID_HANDLER(IDC_CONNECT_BUTTON, OnConnectButton)
        COMMAND_ID_HANDLER(IDC_SERVER_DEFAULT_PORT_CHECK, OnDefaultPortClicked)
        COMMAND_ID_HANDLER(ID_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_HELP, OnHelpButton)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_USERS, OnUsersButton)
        COMMAND_ID_HANDLER(ID_SHOWHIDE, OnShowHideButton)
        COMMAND_ID_HANDLER(ID_INSTALL_SERVICE, OnInstallServiceButton)
        COMMAND_ID_HANDLER(ID_REMOVE_SERVICE, OnRemoveServiceButton)

        COMMAND_HANDLER(IDC_SESSION_TYPE_COMBO, CBN_SELCHANGE, OnSessionTypeChanged)

        CHAIN_MSG_MAP(CTrayIcon<UiMainDialog>)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnDefaultPortClicked(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnStartServerButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSessionTypeChanged(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnSettingsButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnConnectButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    LRESULT OnExitButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAboutButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnUsersButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnHelpButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnShowHideButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnInstallServiceButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnRemoveServiceButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void InitAddressesList();
    void InitSessionTypesCombo();
    proto::SessionType GetSelectedSessionType();
    int AddSessionType(CComboBox& combobox, UINT string_resource_id, proto::SessionType session_type);
    void UpdateSessionType();
    void StopHostMode();

    CIcon small_icon_;
    CIcon big_icon_;

    CMenu main_menu_;

    std::unique_ptr<HostPool> host_pool_;
    std::unique_ptr<ClientPool> client_pool_;
    ClientConfig config_;

    DISALLOW_COPY_AND_ASSIGN(UiMainDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__MAIN_DIALOG_H
