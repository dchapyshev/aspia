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
#include "ui/base/modal_dialog.h"
#include "ui/base/tray_icon.h"

namespace aspia {

class MainDialog : public ModalDialog
{
public:
    MainDialog();
    ~MainDialog() {}

    INT_PTR DoModal(HWND parent) override;

private:
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void OnClose();

    void OnDefaultPortClicked();
    void OnStartServerButton();
    void OnConnectButton();

    void OnExitButton();
    void OnHelpButton();
    void OnAboutButton();
    void OnUsersButton();
    void OnShowHideButton();
    void OnInstallServiceButton();
    void OnRemoveServiceButton();

    void InitAddressesList();
    void InitSessionTypesCombo();
    proto::SessionType GetSelectedSessionType();
    void StopHostMode();

    ScopedHMENU main_menu_;
    TrayIcon tray_icon_;

    std::unique_ptr<HostPool> host_pool_;
    std::unique_ptr<ClientPool> client_pool_;

    DISALLOW_COPY_AND_ASSIGN(MainDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__MAIN_DIALOG_H
