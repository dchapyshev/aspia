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
#include "base/strings/unicode.h"
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

    WriteLog(Module().String(IDS_FT_OP_SESSION_START),
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

        UiSize stop_size = stop_button.Size();
        UiSize minimize_size = minimize_button.Size();

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

void UiFileStatusDialog::OnGetMinMaxInfo(LPMINMAXINFO mmi)
{
    mmi->ptMinTrackSize.x = 350;
    mmi->ptMinTrackSize.y = 200;
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
                    Minimize();
                    break;

                case IDC_STOP_BUTTON:
                    PostMessageW(WM_CLOSE, 0, 0);
                    break;
            }
        }
        break;

        case WM_GETMINMAXINFO:
            OnGetMinMaxInfo(reinterpret_cast<LPMINMAXINFO>(lparam));
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
    CEdit edit(GetDlgItem(IDC_STATUS_EDIT));

    edit.AppendText(CurrentTime().c_str());
    edit.AppendText(L" ");
    edit.AppendText(message.c_str());

    if (status != proto::Status::STATUS_SUCCESS)
    {
        edit.AppendText(L" (");
        edit.AppendText(StatusCodeToString(Module(), status).c_str());
        edit.AppendText(L")");
    }

    edit.AppendText(L"\r\n");
}

void UiFileStatusDialog::SetRequestStatus(const proto::RequestStatus& status)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileStatusDialog::SetRequestStatus,
                                    this,
                                    status));
        return;
    }

    std::wstring message;

    std::wstring first_path = UNICODEfromUTF8(status.first_path());
    std::wstring second_path = UNICODEfromUTF8(status.second_path());

    switch (status.type())
    {
        case proto::RequestStatus::DRIVE_LIST:
        {
            message = Module().String(IDS_FT_OP_BROWSE_DRIVES);
        }
        break;

        case proto::RequestStatus::DIRECTORY_LIST:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_BROWSE_FOLDERS).c_str(),
                                    first_path.c_str());
        }
        break;

        case proto::RequestStatus::CREATE_DIRECTORY:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_CREATE_FOLDER).c_str(),
                                    first_path.c_str());
        }
        break;

        case proto::RequestStatus::RENAME:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_RENAME).c_str(),
                                    first_path.c_str(),
                                    second_path.c_str());
        }
        break;

        case proto::RequestStatus::REMOVE:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_REMOVE).c_str(),
                                    first_path.c_str());
        }
        break;

        case proto::RequestStatus::SEND_FILE:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_SEND_FILE).c_str(),
                                    first_path.c_str());
        }
        break;

        case proto::RequestStatus::RECIEVE_FILE:
        {
            message = StringPrintfW(Module().String(IDS_FT_OP_RECIEVE_FILE).c_str(),
                                    first_path.c_str());
        }
        break;

        default:
        {
            LOG(FATAL) << "Unhandled status code: " << status.type();
        }
        break;
    }

    WriteLog(message, status.code());
}

} // namespace aspia
