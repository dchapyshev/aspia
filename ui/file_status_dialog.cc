//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_status_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_status_dialog.h"
#include "ui/status_code.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "base/strings/string_util.h"
#include "base/logging.h"

#include <filesystem>

namespace aspia {

namespace fs = std::experimental::filesystem;

static const int kBorderSize = 10;

UiFileStatusDialog::UiFileStatusDialog(Delegate* delegate) :
    delegate_(delegate)
{
    DCHECK(delegate_);

    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

UiFileStatusDialog::~UiFileStatusDialog()
{
    ui_thread_.Stop();
}

void UiFileStatusDialog::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    if (!Create(nullptr, IDD_FILE_STATUS, UiModule::Current()))
    {
        LOG(ERROR) << "File status dialog not created";
        runner_->PostQuit();
    }
}

void UiFileStatusDialog::OnAfterThreadRunning()
{
    DestroyWindow();
    delegate_->OnWindowClose();
}

void UiFileStatusDialog::OnInitDialog()
{
    SetTopMost();
    SetIcon(IDI_MAIN);

    WriteLog(module().string(IDS_FT_OP_SESSION_START),
             proto::Status::STATUS_SUCCESS);

    SetFocus(GetDlgItem(IDC_MINIMIZE_BUTTON));
}

void UiFileStatusDialog::OnSize(int width, int height)
{
    HDWP dwp = BeginDeferWindowPos(3);

    if (dwp)
    {
        UiWindow minimize_button(GetDlgItem(IDC_MINIMIZE_BUTTON));
        UiWindow stop_button(GetDlgItem(IDC_STOP_BUTTON));
        UiWindow log_edit(GetDlgItem(IDC_STATUS_EDIT));

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

INT_PTR UiFileStatusDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
                case IDC_MINIMIZE_BUTTON:
                    ShowWindow(hwnd(), SW_MINIMIZE);
                    break;

                case IDC_STOP_BUTTON:
                    PostMessageW(hwnd(), WM_CLOSE, 0, 0);
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
    WCHAR buffer[128];

    if (!GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
        return std::wstring();

    std::wstring datetime(buffer);

    if (!GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, buffer, _countof(buffer)))
        return std::wstring();

    datetime.append(L" ");
    datetime.append(buffer);

    return datetime;
}

void UiFileStatusDialog::WriteLog(const std::wstring& message, proto::Status status)
{
    UiEdit edit(GetDlgItem(IDC_STATUS_EDIT));

    edit.AppendText(CurrentTime());
    edit.AppendText(L" ");
    edit.AppendText(message);

    if (status != proto::Status::STATUS_SUCCESS)
    {
        edit.AppendText(L" (");
        edit.AppendText(StatusCodeToString(module(), status));
        edit.AppendText(L")");
    }

    edit.AppendText(L"\r\n");
}

void UiFileStatusDialog::OnDirectoryOpen(const proto::DirectoryListRequest& request,
                                         proto::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnDirectoryOpen,
                                    this,
                                    request,
                                    status));
        return;
    }

    fs::path path = fs::u8path(request.path());

    if (!request.item().empty())
    {
        if (request.item() == "..")
        {
            if (path.has_parent_path() && path.parent_path() != path.root_name())
            {
                path = path.parent_path();
            }
        }
        else
        {
            path.append(fs::u8path(request.item()));
        }
    }

    WriteLog(StringPrintfW(module().string(IDS_FT_OP_BROWSE_FOLDERS).c_str(),
                           path.c_str()),
             status);
}

void UiFileStatusDialog::OnCreateDirectory(
    const proto::CreateDirectoryRequest& request,
    proto::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnCreateDirectory,
                                    this,
                                    request,
                                    status));
        return;
    }

    std::wstring format = module().string(IDS_FT_OP_CREATE_FOLDER);

    fs::path path = fs::u8path(request.path());
    path.append(fs::u8path(request.name()));

    WriteLog(StringPrintfW(format.c_str(), path.c_str()), status);
}

void UiFileStatusDialog::OnRename(const proto::RenameRequest& request,
                                  proto::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnRename,
                                    this,
                                    request,
                                    status));
        return;
    }

    std::wstring format = module().string(IDS_FT_OP_RENAME);

    fs::path old_path = fs::u8path(request.path());
    old_path.append(fs::u8path(request.old_item_name()));

    fs::path new_path = fs::u8path(request.path());
    new_path.append(fs::u8path(request.new_item_name()));

    WriteLog(StringPrintfW(format.c_str(), old_path.c_str(), new_path.c_str()),
             status);
}

void UiFileStatusDialog::OnRemove(const proto::RemoveRequest& request,
                                  proto::Status status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnRemove,
                                    this,
                                    request,
                                    status));
        return;
    }

    std::wstring format = module().string(IDS_FT_OP_REMOVE);

    fs::path path = fs::u8path(request.path());
    path.append(fs::u8path(request.item_name()));

    WriteLog(StringPrintfW(format.c_str(), path.c_str()), status);
}

void UiFileStatusDialog::OnFileSend(const std::wstring& path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnFileSend, this, path));
        return;
    }
}

void UiFileStatusDialog::OnFileRecieve(const std::wstring& path)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::OnFileRecieve, this, path));
        return;
    }
}

} // namespace aspia
