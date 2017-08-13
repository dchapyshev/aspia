//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/file_manager.h"
#include "ui/file_transfer_dialog.h"
#include "base/logging.h"

namespace aspia {

static const int kDefaultWindowWidth = 980;
static const int kDefaultWindowHeight = 700;
static const int kBorderSize = 3;

FileManagerWindow::FileManagerWindow(std::shared_ptr<FileRequestSenderProxy> local_sender,
                                     std::shared_ptr<FileRequestSenderProxy> remote_sender,
                                     Delegate* delegate)
    : delegate_(delegate),
      local_sender_(local_sender),
      remote_sender_(remote_sender),
      local_panel_(FileManagerPanel::Type::LOCAL, local_sender, this),
      remote_panel_(FileManagerPanel::Type::REMOTE, remote_sender, this)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

FileManagerWindow::~FileManagerWindow()
{
    GUITHREADINFO thread_info;
    thread_info.cbSize = sizeof(thread_info);

    if (GetGUIThreadInfo(ui_thread_.thread_id(), &thread_info))
    {
        ::PostMessageW(thread_info.hwndActive, WM_CLOSE, 0, 0);
    }

    ui_thread_.Stop();
}

void FileManagerWindow::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    CString title;
    title.LoadStringW(IDS_FT_FILE_TRANSFER);

    if (!Create(nullptr, nullptr, title, WS_OVERLAPPEDWINDOW))
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

void FileManagerWindow::OnAfterThreadRunning()
{
    DestroyWindow();
}

void FileManagerWindow::SendFiles(FileManagerPanel::Type panel_type,
                                  const FilePath& source_path,
                                  const FileTransfer::FileList& file_list)
{
    FileTransferDialog::Mode mode;
    FilePath target_path;

    if (panel_type == FileManagerPanel::Type::LOCAL)
    {
        mode = FileTransferDialog::Mode::UPLOAD;
        target_path = remote_panel_.GetCurrentPath();
    }
    else
    {
        DCHECK(panel_type == FileManagerPanel::Type::REMOTE);
        mode = FileTransferDialog::Mode::DOWNLOAD;
        target_path = local_panel_.GetCurrentPath();
    }

    // If the path is empty, the panel displays a list of drives.
    if (source_path.empty() || target_path.empty())
        return;

    FileTransferDialog dialog(mode, remote_sender_, source_path, target_path, file_list);
    dialog.DoModal(*this);

    // Now update the list of files in the panel.
    if (panel_type == FileManagerPanel::Type::LOCAL)
    {
        remote_panel_.Refresh();
    }
    else
    {
        local_panel_.Refresh();
    }
}

LRESULT FileManagerWindow::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
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
    SetIcon(big_icon_, TRUE);

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

    local_panel_.Create(splitter_, nullptr, nullptr, WS_CHILD | WS_VISIBLE);
    remote_panel_.Create(splitter_, nullptr, nullptr, WS_CHILD | WS_VISIBLE);

    splitter_.SetSplitterPane(SPLIT_PANE_LEFT, local_panel_);
    splitter_.SetSplitterPane(SPLIT_PANE_RIGHT, remote_panel_);

    SetWindowPos(nullptr, 0, 0, kDefaultWindowWidth, kDefaultWindowHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    return 0;
}

LRESULT FileManagerWindow::OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    local_panel_.DestroyWindow();
    remote_panel_.DestroyWindow();
    splitter_.DestroyWindow();
    return 0;
}

LRESULT FileManagerWindow::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    CSize size(lparam);
    splitter_.MoveWindow(kBorderSize, 0, size.cx - (kBorderSize * 2), size.cy, FALSE);
    return 0;
}

LRESULT FileManagerWindow::OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT FileManagerWindow::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    delegate_->OnWindowClose();
    return 0;
}

} // namespace aspia
