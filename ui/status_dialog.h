//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_DIALOG_H
#define _ASPIA_UI__STATUS_DIALOG_H

#include "base/scoped_gdi_object.h"
#include "client/client_status.h"
#include "ui/base/modal_dialog.h"

namespace aspia {

class StatusDialog : public ModalDialog
{
public:
    StatusDialog() = default;

    class Delegate
    {
    public:
        virtual void OnStatusDialogOpen() = 0;
    };

    INT_PTR DoModal(HWND parent, Delegate* delegate);
    void SetDestonation(const std::wstring& address, uint16_t port);
    void SetStatus(ClientStatus status);

private:
    INT_PTR DoModal(HWND parent) override;
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void AddMessage(UINT resource_id);

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(StatusDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__STATUS_DIALOG_H
