//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/modal_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/modal_dialog.h"
#include "base/logging.h"

namespace aspia {

ModalDialog::ModalDialog() :
    end_dialog_(false),
    result_(0)
{
    // Nothing
}

INT_PTR ModalDialog::Run(const Module& module, HWND parent, UINT resource_id)
{
    module_ = module;

    HWND disabled_parent = nullptr;

    if (parent)
    {
        if (parent != GetDesktopWindow() && IsWindowEnabled(parent))
        {
            // Modal dialogs exclude interaction with the parent window, disable it.
            disabled_parent = parent;
            EnableWindow(disabled_parent, FALSE);
        }
    }
    else
    {
        parent = GetDesktopWindow();
    }

    // Create a dialog window from the resources.
    if (!CreateDialogParamW(module.Handle(),
                            MAKEINTRESOURCEW(resource_id),
                            parent,
                            DialogProc,
                            reinterpret_cast<LPARAM>(this)))
    {
        LOG(ERROR) << "CreateDialogParamW() failed: " << GetLastError();
        EnableWindow(disabled_parent, TRUE);
        return 0;
    }

    // Now we show the window.
    ShowWindow(hwnd(), SW_SHOWNORMAL);
    UpdateWindow(hwnd());

    // EndDialog called in WM_INITDIALOG
    if (!end_dialog_)
    {
        MessageLoopForUI* current = MessageLoopForUI::Current();

        if (!current)
        {
            DCHECK(!MessageLoop::Current());
            MessageLoopForUI message_loop;
            message_loop.Run(this);
        }
        else
        {
            // Running a nested message loop for modal dialog.
            current->Run(this);
        }
    }

    if (disabled_parent)
    {
        EnableWindow(disabled_parent, TRUE);
        SetForegroundWindow(disabled_parent);
    }

    // After closing the dialog, we destroy the window.
    DestroyWindow(hwnd());
    Attach(nullptr);

    return result_;
}

const Module& ModalDialog::module()
{
    return module_;
}

void ModalDialog::EndDialog(INT_PTR result)
{
    result_ = result;
    end_dialog_ = true;
}

void ModalDialog::SetIcon(UINT resource_id)
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

HWND ModalDialog::GetDlgItem(int item_id)
{
    return ::GetDlgItem(hwnd(), item_id);
}

std::wstring ModalDialog::GetDlgItemString(int item_id)
{
    HWND hwnd = GetDlgItem(item_id);

    if (hwnd)
    {
        // Returns the length without null-character.
        int length = GetWindowTextLengthW(hwnd);
        if (length)
        {
            std::wstring string;
            string.resize(length);

            if (GetWindowTextW(hwnd, &string[0], length + 1))
                return string;
        }
    }

    return std::wstring();
}

bool ModalDialog::SetDlgItemString(int item_id, const std::wstring& string)
{
    return !!SetDlgItemTextW(hwnd(), item_id, string.c_str());
}

bool ModalDialog::SetDlgItemString(int item_id, UINT resource_id)
{
    return SetDlgItemString(item_id, module().string(resource_id));
}

// static
INT_PTR CALLBACK ModalDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ModalDialog* self =
        reinterpret_cast<ModalDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_INITDIALOG)
    {
        self = reinterpret_cast<ModalDialog*>(lparam);
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

bool ModalDialog::Dispatch(const NativeEvent& event)
{
    // If the window was destroyed or the EndDialog method was called.
    if (!IsWindow(hwnd()) || end_dialog_)
        return false; // Finish the current message loop.

    if (!IsDialogMessageW(hwnd(), const_cast<MSG*>(&event)))
    {
        TranslateMessage(&event);
        DispatchMessageW(&event);
    }

    return true;
}

} // namespace aspia
