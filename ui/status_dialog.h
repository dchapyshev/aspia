//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/status_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__STATUS_DIALOG_H
#define _ASPIA_UI__STATUS_DIALOG_H

#include "base/scoped_gdi_object.h"
#include "proto/status.pb.h"
#include "ui/base/modal_dialog.h"

namespace aspia {

class UiStatusDialog : public UiModalDialog
{
public:
    UiStatusDialog() = default;

    class Delegate
    {
    public:
        virtual void OnStatusDialogOpen() = 0;
    };

    INT_PTR DoModal(HWND parent, Delegate* delegate);
    void SetDestonation(const std::wstring& address, uint16_t port);
    void SetStatus(proto::Status status);

private:
    INT_PTR DoModal(HWND parent) override;
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void AddMessage(const std::wstring& message);

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UiStatusDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__STATUS_DIALOG_H
