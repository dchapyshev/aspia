//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info_window.h"
#include "ui/resource.h"
#include "base/logging.h"

#include <atlmisc.h>

namespace aspia {

static const int kDefaultWindowWidth = 980;
static const int kDefaultWindowHeight = 700;
static const int kBorderSize = 3;

SystemInfoWindow::SystemInfoWindow(Delegate* delegate)
    : delegate_(delegate)
{
    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

SystemInfoWindow::~SystemInfoWindow()
{
    ui_thread_.Stop();
}

void SystemInfoWindow::OnBeforeThreadRunning()
{
    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    CString title;
    title.LoadStringW(IDS_SI_SYSTEM_INFORMATION);

    if (!Create(nullptr, rcDefault, title, WS_OVERLAPPEDWINDOW))
    {
        LOG(ERROR) << "System information window not created: " << GetLastSystemErrorString();
        runner_->PostQuit();
    }
    else
    {
        ShowWindow(SW_SHOW);
        UpdateWindow();
    }
}

void SystemInfoWindow::OnThreadRunning(MessageLoop* message_loop)
{
    // Run message loop with this message dispatcher.
    message_loop->Run(this);
}

void SystemInfoWindow::OnAfterThreadRunning()
{
    DestroyWindow();
}

bool SystemInfoWindow::Dispatch(const NativeEvent& event)
{
    TranslateMessage(&event);
    DispatchMessageW(&event);
    return true;
}

LRESULT SystemInfoWindow::OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
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
    splitter_.SetSplitterPos(250);
    splitter_.m_cxySplitBar = 5;
    splitter_.m_cxyMin = 0;
    splitter_.m_bFullDrag = false;

    SetWindowPos(nullptr, 0, 0, kDefaultWindowWidth, kDefaultWindowHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    CenterWindow();

    return 0;
}

LRESULT SystemInfoWindow::OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    splitter_.DestroyWindow();
    return 0;
}

LRESULT SystemInfoWindow::OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    CSize size(lparam);
    splitter_.MoveWindow(kBorderSize, 0, size.cx - (kBorderSize * 2), size.cy, FALSE);
    return 0;
}

LRESULT SystemInfoWindow::OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lparam);

    mmi->ptMinTrackSize.x = 500;
    mmi->ptMinTrackSize.y = 400;

    return 0;
}

LRESULT SystemInfoWindow::OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled)
{
    delegate_->OnWindowClose();
    return 0;
}

} // namespace aspia
