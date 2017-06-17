//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/modal_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__MODAL_DIALOG_H
#define _ASPIA_UI__BASE__MODAL_DIALOG_H

#include "base/message_loop/message_loop.h"
#include "base/scoped_user_object.h"
#include "ui/base/dialog.h"

namespace aspia {

class ModalDialog :
    public Dialog,
    private MessageLoopForUI::Dispatcher
{
public:
    ModalDialog() = default;
    virtual ~ModalDialog() = default;

    virtual INT_PTR DoModal(HWND parent) = 0;

    // Like EndDialog function from Windows API, but to work with
    // current modal dialog implementation.
    void EndDialog(INT_PTR result = 0);

protected:
    // Runs the modal dialog in the current message loop.
    INT_PTR Run(const Module& module, HWND parent, UINT resource_id);

private:
    bool Dispatch(const NativeEvent& event) override;

    bool end_dialog_ = false;
    INT_PTR result_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ModalDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__MODAL_DIALOG_H
