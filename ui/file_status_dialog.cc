//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_status_dialog.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "ui/resource.h"
#include "base/logging.h"

namespace aspia {

static const int kBorderSize = 10;

FileStatusDialog::FileStatusDialog(Delegate* delegate) :
    delegate_(delegate)
{
    DCHECK(delegate_);

    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

FileStatusDialog::~FileStatusDialog()
{
    ui_thread_.Stop();
}

void FileStatusDialog::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    if (!Create(nullptr, IDD_FILE_STATUS, Module::Current()))
    {
        LOG(ERROR) << "File status dialog not created";
        runner_->PostQuit();
    }
}

void FileStatusDialog::OnAfterThreadRunning()
{
    DestroyWindow();
    delegate_->OnWindowClose();
}

void FileStatusDialog::OnInitDialog()
{
    SetWindowPos(hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    SetIcon(IDI_MAIN);
}

void FileStatusDialog::OnSize(int width, int height)
{
    HDWP dwp = BeginDeferWindowPos(3);

    if (dwp)
    {
        Window minimize_button(GetDlgItem(IDC_MINIMIZE_BUTTON));
        Window stop_button(GetDlgItem(IDC_STOP_BUTTON));
        Window log_edit(GetDlgItem(IDC_STATUS_EDIT));

        DesktopSize stop_size = stop_button.Size();
        DesktopSize minimize_size = minimize_button.Size();

        DeferWindowPos(dwp,
                       log_edit,
                       nullptr,
                       kBorderSize,
                       kBorderSize,
                       width - (kBorderSize * 2),
                       height - (kBorderSize * 3) - stop_size.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp,
                       stop_button,
                       nullptr,
                       width - (stop_size.Width() + kBorderSize),
                       height - (stop_size.Height() + kBorderSize),
                       stop_size.Width(),
                       stop_size.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        DeferWindowPos(dwp,
                       minimize_button,
                       nullptr,
                       width - (stop_size.Width() + minimize_size.Width() + (kBorderSize * 2)),
                       height - (minimize_size.Height() + kBorderSize),
                       minimize_size.Width(),
                       minimize_size.Height(),
                       SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }
}

INT_PTR FileStatusDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_SIZE:
            OnSize(LOWORD(lparam), HIWORD(lparam));
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDCANCEL:
                    break;
            }
        }
        break;

        case WM_CLOSE:
            PostQuitMessage(0);
            break;
    }

    return 0;
}

static std::wstring CurrentTime()
{
    std::wstring time;

    WCHAR buffer[128];

    if (!GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
        return std::wstring();

    return buffer;
}

void FileStatusDialog::OnDirectoryOpen(const std::wstring& path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileStatusDialog::OnDirectoryOpen, this, path));
        return;
    }

    Edit log_edit(GetDlgItem(IDC_STATUS_EDIT));

    log_edit.AppendText(CurrentTime());
    log_edit.AppendText(L" Browse folders: ");
    log_edit.AppendText(path);
    log_edit.AppendText(L"\r\n");
}

void FileStatusDialog::OnFileSend(const std::wstring& path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileStatusDialog::OnFileSend, this, path));
        return;
    }
}

void FileStatusDialog::OnFileRecieve(const std::wstring& path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&FileStatusDialog::OnFileRecieve, this, path));
        return;
    }
}

} // namespace aspia
