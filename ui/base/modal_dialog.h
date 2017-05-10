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
#include "ui/base/window.h"
#include "ui/base/module.h"

namespace aspia {

class ModalDialog :
    public Window,
    private MessageLoopForUI::Dispatcher
{
public:
    ModalDialog() = default;
    virtual ~ModalDialog() = default;

    virtual INT_PTR DoModal(HWND parent) = 0;

    // Returns module instance from which the dialog is loaded.
    const Module& module();

    // Like EndDialog function from Windows API, but to work with
    // current modal dialog implementation.
    void EndDialog(INT_PTR result);

    void SetIcon(UINT resource_id);

    HWND GetDlgItem(int item_id);

    std::wstring GetDlgItemString(int item_id);
    bool SetDlgItemString(int item_id, const std::wstring& string);
    bool SetDlgItemString(int item_id, UINT resource_id);

    template <typename T>
    T GetDlgItemInt(int item_id)
    {
        const bool is_signed = std::numeric_limits<T>::is_signed;
        return ::GetDlgItemInt(hwnd(), item_id, nullptr, is_signed);
    }

    template <typename T>
    bool SetDlgItemInt(int item_id, T value)
    {
        const bool is_signed = std::numeric_limits<T>::is_signed;
        return !!::SetDlgItemInt(hwnd(), item_id, value, is_signed);
    }

protected:
    // Runs the modal dialog in the current message loop.
    INT_PTR Run(const Module& module, HWND parent, UINT resource_id);

    virtual INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) = 0;

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    bool Dispatch(const NativeEvent& event) override;

    Module module_;
    bool end_dialog_ = false;
    INT_PTR result_ = 0;

    ScopedHICON small_icon_;
    ScopedHICON big_icon_;

    DISALLOW_COPY_AND_ASSIGN(ModalDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__MODAL_DIALOG_H
