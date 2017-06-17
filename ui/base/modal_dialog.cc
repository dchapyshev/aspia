//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/modal_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/modal_dialog.h"
#include "base/logging.h"

namespace aspia {

INT_PTR UiModalDialog::Run(const UiModule& module, HWND parent, UINT resource_id)
{
    end_dialog_ = false;
    result_ = 0;

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
    if (!Create(parent, resource_id, module))
    {
        LOG(ERROR) << "CreateDialogParamW() failed: "
                   << GetLastSystemErrorString();
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
    DestroyWindow();

    return result_;
}

void UiModalDialog::EndDialog(INT_PTR result)
{
    result_ = result;
    end_dialog_ = true;
}

bool UiModalDialog::Dispatch(const NativeEvent& event)
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
