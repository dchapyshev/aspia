//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/dialog.h"
#include "base/logging.h"

namespace aspia {

bool UiDialog::Create(HWND parent, UINT resource_id, const UiModule& module)
{
    module_ = module;

    // Create a dialog window from the resources.
    if (!CreateDialogParamW(module.Handle(),
                            MAKEINTRESOURCEW(resource_id),
                            parent,
                            DialogProc,
                            reinterpret_cast<LPARAM>(this)))
    {
        LOG(ERROR) << "CreateDialogParamW() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    // Now we show the window.
    ShowNormal();
    UpdateWindow(hwnd());

    return true;
}

// static
INT_PTR CALLBACK UiDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    UiDialog* self =
        reinterpret_cast<UiDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_INITDIALOG)
    {
        self = reinterpret_cast<UiDialog*>(lparam);
        self->Attach(hwnd);

        // Store pointer to the self to the window's user data.
        SetLastError(ERROR_SUCCESS);
        LONG_PTR result = SetWindowLongPtrW(hwnd,
                                            GWLP_USERDATA,
                                            reinterpret_cast<LONG_PTR>(self));
        CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
    }

    if (self)
        return self->OnMessage(msg, wparam, lparam);

    return 0;
}

const UiModule& UiDialog::Module()
{
    return module_;
}

void UiDialog::SetIcon(UINT resource_id)
{
    small_icon_ = Module().Icon(resource_id,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON),
                                LR_CREATEDIBSECTION);

    big_icon_ = Module().Icon(resource_id,
                              GetSystemMetrics(SM_CXICON),
                              GetSystemMetrics(SM_CYICON),
                              LR_CREATEDIBSECTION);

    SendMessageW(WM_SETICON, FALSE, reinterpret_cast<LPARAM>(small_icon_.Get()));
    SendMessageW(WM_SETICON, TRUE, reinterpret_cast<LPARAM>(big_icon_.Get()));
}

HWND UiDialog::GetDlgItem(int item_id)
{
    return ::GetDlgItem(hwnd(), item_id);
}

void UiDialog::EnableDlgItem(int item_id, bool enable)
{
    EnableWindow(GetDlgItem(item_id), enable);
}

std::wstring UiDialog::GetDlgItemString(int item_id)
{
    return UiWindow(GetDlgItem(item_id)).GetWindowString();
}

void UiDialog::SetDlgItemString(int item_id, const WCHAR* string)
{
    SetDlgItemTextW(hwnd(), item_id, string);
}

void UiDialog::SetDlgItemString(int item_id, const std::wstring& string)
{
    SetDlgItemString(item_id, string.c_str());
}

void UiDialog::SetDlgItemString(int item_id, UINT resource_id)
{
    SetDlgItemString(item_id, Module().String(resource_id));
}

UINT UiDialog::IsDlgButtonChecked(int button_id)
{
    return ::IsDlgButtonChecked(hwnd(), button_id);
}

void UiDialog::CheckDlgButton(int button_id, UINT check)
{
    ::CheckDlgButton(hwnd(), button_id, check);
}

} // namespace aspia
