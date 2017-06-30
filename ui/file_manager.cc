//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager.h"
#include "ui/resource.h"
#include "ui/status_code.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

static const int kDefaultWindowWidth = 980;
static const int kDefaultWindowHeight = 700;
static const int kBorderSize = 3;

UiFileManager::UiFileManager(Delegate* delegate) :
    delegate_(delegate)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

UiFileManager::~UiFileManager()
{
    ui_thread_.Stop();
}

void UiFileManager::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    CString title;
    title.LoadStringW(IDS_FT_FILE_TRANSFER);

    if (!Create(nullptr, 0, title, style))
    {
        LOG(ERROR) << "File manager window not created";
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOW);
    }
}

void UiFileManager::OnAfterThreadRunning()
{
    DestroyWindow();
}

void UiFileManager::ReadRequestStatus(std::shared_ptr<proto::RequestStatus> status)
{
    DCHECK(status);

    if (status->code() == proto::Status::STATUS_SUCCESS)
        return;

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&UiFileManager::ReadRequestStatus, this, status));
        return;
    }

    CString status_code = StatusCodeToString(status->code());
    std::wstring first_path = UNICODEfromUTF8(status->first_path());
    std::wstring second_path = UNICODEfromUTF8(status->second_path());

    CString message;

    switch (status->type())
    {
        case proto::RequestStatus::DRIVE_LIST:
        {
            message.Format(IDS_FT_OP_BROWSE_DRIVES_ERROR,
                           status_code);
        }
        break;

        case proto::RequestStatus::DIRECTORY_LIST:
        {
            message.Format(IDS_FT_BROWSE_FOLDERS_ERROR,
                           first_path.c_str(),
                           status_code);
        }
        break;

        case proto::RequestStatus::CREATE_DIRECTORY:
        {
            message.Format(IDS_FT_OP_CREATE_FOLDER_ERROR,
                           first_path.c_str(),
                           status_code);
        }
        break;

        case proto::RequestStatus::RENAME:
        {
            message.Format(IDS_FT_OP_RENAME_ERROR,
                           first_path.c_str(),
                           second_path.c_str(),
                           status_code);
        }
        break;

        case proto::RequestStatus::REMOVE:
        {
            message.Format(IDS_FT_OP_REMOVE_ERROR,
                           first_path.c_str(),
                           status_code);
        }
        break;

        case proto::RequestStatus::SEND_FILE:
        {
            message.Format(IDS_FT_OP_SEND_FILE_ERROR,
                           first_path.c_str(),
                           status_code);
        }
        break;

        case proto::RequestStatus::RECIEVE_FILE:
        {
            message.Format(IDS_FT_OP_RECIEVE_FILE_ERROR,
                           first_path.c_str(),
                           status_code);
        }
        break;

        default:
        {
            DLOG(FATAL) << "Unhandled status code: " << status->type();
        }
        break;
    }

    MessageBoxW(message, nullptr, MB_ICONWARNING | MB_OK);
}

void UiFileManager::ReadDriveList(PanelType panel_type,
                                  std::unique_ptr<proto::DriveList> drive_list)
{
    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        remote_panel_.ReadDriveList(std::move(drive_list));
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        local_panel_.ReadDriveList(std::move(drive_list));
    }
}

void UiFileManager::ReadDirectoryList(PanelType panel_type,
                                      std::unique_ptr<proto::DirectoryList> directory_list)
{
    if (panel_type == UiFileManager::PanelType::REMOTE)
    {
        remote_panel_.ReadDirectoryList(std::move(directory_list));
    }
    else
    {
        DCHECK(panel_type == UiFileManager::PanelType::LOCAL);
        local_panel_.ReadDirectoryList(std::move(directory_list));
    }
}

void UiFileManager::OnDriveListRequest(UiFileManager::PanelType panel_type)
{
    delegate_->OnDriveListRequest(panel_type);
}

void UiFileManager::OnDirectoryListRequest(PanelType panel_type,
                                           const std::string& path,
                                           const std::string& item)
{
    delegate_->OnDirectoryListRequest(panel_type, path, item);
}

void UiFileManager::OnCreateDirectoryRequest(PanelType panel_type,
                                             const std::string& path,
                                             const std::string& name)
{
    delegate_->OnCreateDirectoryRequest(panel_type, path, name);
}

void UiFileManager::OnRenameRequest(PanelType panel_type,
                                    const std::string& path,
                                    const std::string& old_name,
                                    const std::string& new_name)
{
    delegate_->OnRenameRequest(panel_type, path, old_name, new_name);
}

void UiFileManager::OnRemoveRequest(PanelType panel_type,
                                    const std::string& path,
                                    const std::string& item_name)
{
    delegate_->OnRemoveRequest(panel_type, path, item_name);
}

LRESULT UiFileManager::OnCreate(UINT message,
                                WPARAM wparam,
                                LPARAM lparam,
                                BOOL& handled)
{
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    small_icon_ = AtlLoadIconImage(IDI_MAIN,
                                   LR_CREATEDIBSECTION,
                                   GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON));
    SetIcon(small_icon_, FALSE);

    big_icon_ = AtlLoadIconImage(IDI_MAIN,
                                 LR_CREATEDIBSECTION,
                                 GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON));
    SetIcon(small_icon_, TRUE);

    CRect client_rect;
    GetClientRect(client_rect);

    splitter_.Create(*this, client_rect, nullptr,
                     WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);

    splitter_.SetActivePane(SPLIT_PANE_LEFT);
    splitter_.SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
    splitter_.SetSplitterPos(-1);
    splitter_.m_cxySplitBar = 5;
    splitter_.m_cxyMin = 0;
    splitter_.m_bFullDrag = false;

    local_panel_.CreatePanel(splitter_, UiFileManager::PanelType::LOCAL, this);
    remote_panel_.CreatePanel(splitter_, UiFileManager::PanelType::REMOTE, this);

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, local_panel_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, remote_panel_);

    SetWindowPos(nullptr, 0, 0, kDefaultWindowWidth, kDefaultWindowHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    return 0;
}

LRESULT UiFileManager::OnDestroy(UINT message,
                                 WPARAM wparam,
                                 LPARAM lparam,
                                 BOOL& handled)
{
    local_panel_.DestroyWindow();
    remote_panel_.DestroyWindow();
    splitter_.DestroyWindow();
    return 0;
}

LRESULT UiFileManager::OnSize(UINT message,
                              WPARAM wparam,
                              LPARAM lparam,
                              BOOL& handled)
{
    HDWP dwp = BeginDeferWindowPos(1);

    if (dwp)
    {
        int width = LOWORD(lparam);
        int height = HIWORD(lparam);

        splitter_.DeferWindowPos(dwp,
                                 nullptr,
                                 kBorderSize,
                                 kBorderSize,
                                 width - (kBorderSize * 2),
                                 height - (kBorderSize * 2),
                                 SWP_NOACTIVATE | SWP_NOZORDER);

        EndDeferWindowPos(dwp);
    }

    return 0;
}

LRESULT UiFileManager::OnGetMinMaxInfo(UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL& handled)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT UiFileManager::OnClose(UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               BOOL& handled)
{
    delegate_->OnWindowClose();
    return 0;
}

} // namespace aspia
