//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/dialog.h"
#include "base/logging.h"

namespace aspia {

bool Dialog::Create(HWND parent, UINT resource_id, const Module& module)
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
    ShowWindow(hwnd(), SW_SHOWNORMAL);
    UpdateWindow(hwnd());

    return true;
}

// static
INT_PTR CALLBACK Dialog::DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Dialog* self =
        reinterpret_cast<Dialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_INITDIALOG)
    {
        self = reinterpret_cast<Dialog*>(lparam);
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

const Module& Dialog::module()
{
    return module_;
}

void Dialog::SetIcon(UINT resource_id)
{
    small_icon_ = module().icon(resource_id,
                                GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON),
                                LR_CREATEDIBSECTION);

    big_icon_ = module().icon(resource_id,
                              GetSystemMetrics(SM_CXICON),
                              GetSystemMetrics(SM_CYICON),
                              LR_CREATEDIBSECTION);

    SendMessageW(hwnd(), WM_SETICON, FALSE, reinterpret_cast<LPARAM>(small_icon_.Get()));
    SendMessageW(hwnd(), WM_SETICON, TRUE, reinterpret_cast<LPARAM>(big_icon_.Get()));
}

HWND Dialog::GetDlgItem(int item_id)
{
    return ::GetDlgItem(hwnd(), item_id);
}

void Dialog::EnableDlgItem(int item_id, bool enable)
{
    EnableWindow(GetDlgItem(item_id), enable);
}

std::wstring Dialog::GetDlgItemString(int item_id)
{
    HWND hwnd = GetDlgItem(item_id);

    if (hwnd)
    {
        // Returns the length without null-character.
        int length = GetWindowTextLengthW(hwnd);
        if (length > 0)
        {
            std::wstring string;
            string.resize(length);

            if (GetWindowTextW(hwnd, &string[0], length + 1))
                return string;
        }
    }

    return std::wstring();
}

bool Dialog::SetDlgItemString(int item_id, const std::wstring& string)
{
    return !!SetDlgItemTextW(hwnd(), item_id, string.c_str());
}

bool Dialog::SetDlgItemString(int item_id, UINT resource_id)
{
    return SetDlgItemString(item_id, module().string(resource_id));
}

} // namespace aspia
