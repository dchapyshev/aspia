//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__DIALOG_H
#define _ASPIA_UI__BASE__DIALOG_H

#include "ui/base/window.h"
#include "ui/base/module.h"

namespace aspia {

class UiDialog : public UiWindow
{
public:
    UiDialog() = default;
    ~UiDialog() = default;

    bool Create(HWND parent, UINT resource_id, const UiModule& module);

    // Returns module instance from which the dialog is loaded.
    const UiModule& Module();

    void SetIcon(UINT resource_id);

    HWND GetDlgItem(int item_id);
    void EnableDlgItem(int item_id, bool enable);

    std::wstring GetDlgItemString(int item_id);
    bool SetDlgItemString(int item_id, const std::wstring& string);
    bool SetDlgItemString(int item_id, UINT resource_id);

    template <typename T>
    T GetDlgItemInt(int item_id)
    {
        const bool is_signed = std::numeric_limits<T>::is_signed;
        return static_cast<T>(::GetDlgItemInt(hwnd(), item_id, nullptr, is_signed));
    }

    template <typename T>
    bool SetDlgItemInt(int item_id, T value)
    {
        const bool is_signed = std::numeric_limits<T>::is_signed;
        return !!::SetDlgItemInt(hwnd(), item_id, value, is_signed);
    }

protected:
    virtual INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) = 0;

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    UiModule module_;
    ScopedHICON small_icon_;
    ScopedHICON big_icon_;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__DIALOG_H
