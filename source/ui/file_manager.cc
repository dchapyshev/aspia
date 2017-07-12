//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager.h"
#include "ui/file_transfer_dialog.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

static const int kDefaultWindowWidth = 980;
static const int kDefaultWindowHeight = 700;
static const int kBorderSize = 3;

UiFileManager::UiFileManager(std::shared_ptr<FileRequestSenderProxy> local_sender,
                             std::shared_ptr<FileRequestSenderProxy> remote_sender,
                             Delegate* delegate) :
    delegate_(delegate),
    local_sender_(local_sender),
    remote_sender_(remote_sender),
    local_panel_(UiFileManagerPanel::PanelType::LOCAL, local_sender, this),
    remote_panel_(UiFileManagerPanel::PanelType::REMOTE, remote_sender, this)
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

    CString title;
    title.LoadStringW(IDS_FT_FILE_TRANSFER);

    if (!Create(nullptr, 0, title, WS_OVERLAPPEDWINDOW))
    {
        LOG(ERROR) << "File manager window not created";
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOW);
        UpdateWindow();
    }
}

void UiFileManager::OnAfterThreadRunning()
{
    DestroyWindow();
}

void UiFileManager::SendFiles(UiFileManagerPanel::PanelType panel_type,
                              const FilePath& source_path,
                              const FileTransfer::FileList& file_list)
{
    UiFileTransferDialog::Mode mode;
    FilePath target_path;

    if (panel_type == UiFileManagerPanel::PanelType::LOCAL)
    {
        mode = UiFileTransferDialog::Mode::UPLOAD;
        target_path = remote_panel_.GetCurrentPath();
    }
    else
    {
        DCHECK(panel_type == UiFileManagerPanel::PanelType::REMOTE);
        mode = UiFileTransferDialog::Mode::DOWNLOAD;
        target_path = local_panel_.GetCurrentPath();
    }

    // If the path is empty, the panel displays a list of drives.
    if (source_path.empty() || target_path.empty())
        return;

    UiFileTransferDialog dialog(mode, remote_sender_, source_path, target_path, file_list);
    dialog.DoModal(*this);
}

LRESULT UiFileManager::OnCreate(UINT message,
                                WPARAM wparam,
                                LPARAM lparam,
                                BOOL& handled)
{
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

    local_panel_.Create(splitter_, 0, 0, WS_CHILD | WS_VISIBLE);
    remote_panel_.Create(splitter_, 0, 0, WS_CHILD | WS_VISIBLE);

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
    int width = LOWORD(lparam);
    int height = HIWORD(lparam);

    splitter_.MoveWindow(kBorderSize,
                         0,
                         width - (kBorderSize * 2),
                         height,
                         FALSE);
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
