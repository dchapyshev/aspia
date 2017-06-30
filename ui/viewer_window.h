//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/viewer_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__VIEWER_WINDOW_H
#define _ASPIA_UI__VIEWER_WINDOW_H

#include "base/message_loop/message_loop_thread.h"
#include "client/client_config.h"
#include "protocol/clipboard.h"
#include "ui/video_window.h"
#include "ui/about_dialog.h"
#include "ui/settings_dialog.h"
#include "ui/base/toolbar.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiViewerWindow :
    public UiChildWindow,
    private UiVideoWindow::Delegate,
    private MessageLoopThread::Delegate
{
public:
    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
        virtual void OnConfigChange(const proto::DesktopSessionConfig& config) = 0;
        virtual void OnKeyEvent(uint32_t keycode, uint32_t flags) = 0;
        virtual void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) = 0;
        virtual void OnPowerEvent(proto::PowerEvent::Action action) = 0;
        virtual void OnClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event) = 0;
    };

    UiViewerWindow(ClientConfig* config, Delegate* delegate);
    ~UiViewerWindow();

    DesktopFrame* Frame();
    void DrawFrame();
    void ResizeFrame(const DesktopSize& size, const PixelFormat& format);
    void InjectMouseCursor(std::shared_ptr<MouseCursor> mouse_cursor);
    void InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event);

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // UiChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    // UiVideoWindow::Delegate implementation.
    void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) override;

    void OnCreate();
    void OnClose();
    void OnSize();
    void OnGetMinMaxInfo(LPMINMAXINFO mmi);
    void OnActivate(UINT state);
    void OnKeyboard(WPARAM wParam, LPARAM lParam);
    void OnSetFocus();
    void OnKillFocus();
    void OnSettingsButton();
    void OnAboutButton();
    void OnExitButton();
    void OnAutoSizeButton();
    void OnFullScreenButton();
    void OnDropDownButton(WORD ctrl_id);
    void OnPowerButton();
    void OnCADButton();
    void OnKeyButton(WORD ctrl_id);
    void OnGetDispInfo(LPNMHDR phdr);
    void OnToolBarDropDown(LPNMHDR phdr);
    void OnVideoFrameResize(WPARAM wParam, LPARAM lParam);

    void AddToolBarIcon(UINT icon_id);
    void CreateToolBar();
    void ShowDropDownMenu(int button_id, RECT* button_rect);
    int DoAutoSize(const DesktopSize& video_frame_size);
    void DoFullScreen(bool fullscreen);
    void ApplyConfig(const proto::DesktopSessionConfig& config);

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;
    ClientConfig* config_;

    UiToolBar toolbar_;
    CImageListManaged toolbar_imagelist_;

    UiVideoWindow video_window_;
    Clipboard clipboard_;
    ScopedHHOOK keyboard_hook_;
    WINDOWPLACEMENT window_pos_ = { 0 };

    DISALLOW_COPY_AND_ASSIGN(UiViewerWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__VIEWER_WINDOW_H
