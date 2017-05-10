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
#include "ui/base/imagelist.h"

namespace aspia {

class ViewerWindow :
    public ChildWindow,
    private VideoWindow::Delegate,
    private MessageLoopThread::Delegate
{
public:
    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
        virtual void OnConfigChange(const proto::DesktopSessionConfig& config) = 0;
        virtual void OnKeyEvent(uint32_t keycode, uint32_t flags) = 0;
        virtual void OnPointerEvent(int x, int y, uint32_t mask) = 0;
        virtual void OnPowerEvent(proto::PowerEvent::Action action) = 0;
        virtual void OnClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event) = 0;
    };

    ViewerWindow(const ClientConfig& config, Delegate* delegate);
    ~ViewerWindow();

    DesktopFrame* Frame();
    void DrawFrame();
    void ResizeFrame(const DesktopSize& size, const PixelFormat& format);
    void InjectMouseCursor(std::shared_ptr<MouseCursor> mouse_cursor);
    void InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event);

private:
    // MessageThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // ChildWindow implementation.
    bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) override;

    // VideoWindow::Delegate implementation.
    void OnPointerEvent(int32_t x, int32_t y, uint32_t mask) override;

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
    void OnPowerButton(WORD ctrl_id);
    void OnCADButton();
    void OnKeyButton(WORD ctrl_id);
    void OnGetDispInfo(LPNMHDR phdr);
    void OnToolBarDropDown(LPNMHDR phdr);

    void CreateToolBar();
    void ShowDropDownMenu(int button_id, RECT* button_rect);
    int DoAutoSize(const DesktopSize& video_frame_size);
    void DoFullScreen(bool fullscreen);
    void ApplyConfig(const proto::DesktopSessionConfig& config);

private:
    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;
    ClientConfig config_;

    Window toolbar_;
    ImageList toolbar_imagelist_;

    VideoWindow video_window_;
    Clipboard clipboard_;
    ScopedHHOOK keyboard_hook_;
    WINDOWPLACEMENT window_pos_;

    DISALLOW_COPY_AND_ASSIGN(ViewerWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__VIEWER_WINDOW_H
