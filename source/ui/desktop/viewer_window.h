//
// PROJECT:         Aspia
// FILE:            ui/desktop/viewer_window.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__DESKTOP__VIEWER_WINDOW_H
#define _ASPIA_UI__DESKTOP__VIEWER_WINDOW_H

#include "base/message_loop/message_loop_thread.h"
#include "base/scoped_user_object.h"
#include "client/client_config.h"
#include "protocol/clipboard.h"
#include "ui/desktop/viewer_toolbar.h"
#include "ui/desktop/video_window.h"
#include "ui/desktop/settings_dialog.h"

namespace aspia {

class ViewerWindow :
    public CWindowImpl<ViewerWindow, CWindow>,
    private VideoWindow::Delegate,
    private MessageLoopThread::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnWindowClose() = 0;
        virtual void OnConfigChange(const proto::desktop::Config& config) = 0;
        virtual void OnKeyEvent(uint32_t keycode, uint32_t flags) = 0;
        virtual void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) = 0;
        virtual void OnClipboardEvent(proto::desktop::ClipboardEvent& clipboard_event) = 0;
    };

    ViewerWindow(ClientConfig* config, Delegate* delegate);
    ~ViewerWindow();

    DesktopFrame* Frame();
    void DrawFrame();
    void ResizeFrame(const DesktopSize& size, const PixelFormat& format);
    void InjectMouseCursor(std::shared_ptr<MouseCursor> mouse_cursor);
    void InjectClipboardEvent(std::shared_ptr<proto::desktop::ClipboardEvent> clipboard_event);

private:
    static const UINT kResizeFrameMessage = WM_APP + 1;

    BEGIN_MSG_MAP(ViewerWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_KEYDOWN, OnKeyboard)
        MESSAGE_HANDLER(WM_KEYUP, OnKeyboard)
        MESSAGE_HANDLER(WM_SYSKEYDOWN, OnKeyboard)
        MESSAGE_HANDLER(WM_SYSKEYUP, OnKeyboard)
        MESSAGE_HANDLER(WM_CHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_SYSCHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_DEADCHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_SYSDEADCHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_ACTIVATE, OnActivate)
        MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
        MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(kResizeFrameMessage, OnVideoFrameResize)

        NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnToolBarDropDown)

        COMMAND_ID_HANDLER(ID_SETTINGS, OnSettingsButton)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_AUTO_SIZE, OnAutoSizeButton)
        COMMAND_ID_HANDLER(ID_FULLSCREEN, OnFullScreenButton)
        COMMAND_ID_HANDLER(ID_SHORTCUTS, OnDropDownButton)
        COMMAND_ID_HANDLER(ID_CAD, OnCADButton)

        COMMAND_RANGE_HANDLER(ID_KEY_FIRST, ID_KEY_LAST, OnKeyButton)
    END_MSG_MAP()

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // VideoWindow::Delegate implementation.
    void OnPointerEvent(const DesktopPoint& pos, uint32_t mask) override;

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnActivate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnKeyboard(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSkipMessage(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnKillFocus(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnVideoFrameResize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnSettingsButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAboutButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnExitButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnAutoSizeButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnFullScreenButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnDropDownButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCADButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnKeyButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnToolBarDropDown(int control_id, LPNMHDR hdr, BOOL& handled);

    void ShowDropDownMenu(int button_id, RECT* button_rect);

    // Returns the size of the monitor on which the window is located.
    CSize GetMonitorSize() const;

    // Calculates the window size for the specified video frame size.
    CSize CalculateWindowSize(const DesktopSize &video_frame_size) const;

    int DoAutoSize(const DesktopSize& video_frame_size);
    void DoFullScreen(bool fullscreen);
    void ApplyConfig(const proto::desktop::Config& config);

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;
    ClientConfig* config_;

    ViewerToolBar toolbar_;
    VideoWindow video_window_;

    Clipboard clipboard_;
    ScopedHHOOK keyboard_hook_;
    WINDOWPLACEMENT window_pos_ = { 0 };

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(ViewerWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__VIEWER_WINDOW_H
