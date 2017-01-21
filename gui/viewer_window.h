//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/viewer_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__VIEWER_WINDOW_H
#define _ASPIA_GUI__VIEWER_WINDOW_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "base/thread.h"

#include "client/client.h"

#include "gui/viewer_bar.h"
#include "gui/video_window.h"
#include "gui/about_dialog.h"
#include "gui/settings_dialog.h"
#include "gui/scoped_windows_hook.h"

namespace aspia {

class ViewerWindow :
    public CWindowImpl <ViewerWindow, CWindow, CFrameWinTraits>,
    private Thread
{
public:
    BEGIN_MSG_MAP(ViewerWindow)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_CLOSE(OnClose)
        MSG_WM_SIZE(OnSize)
        MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
        MSG_WM_ACTIVATE(OnActivate)
        MSG_WM_SETFOCUS(OnSetFocus)
        MSG_WM_KILLFOCUS(OnKillFocus)

        MESSAGE_RANGE_HANDLER(WM_KEYDOWN, WM_KEYUP, OnKeyboard)
        MESSAGE_RANGE_HANDLER(WM_SYSKEYDOWN, WM_SYSKEYUP, OnKeyboard)

        MESSAGE_HANDLER(WM_MOUSEWHEEL, video_window_.OnMouse)

        MESSAGE_HANDLER(WM_CHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_SYSCHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_DEADCHAR, OnSkipMessage)
        MESSAGE_HANDLER(WM_SYSDEADCHAR, OnSkipMessage)

        COMMAND_ID_HANDLER(ID_SETTINGS, OnSettingsButton)
        COMMAND_ID_HANDLER(ID_BELL, OnBellButton)
        COMMAND_ID_HANDLER(ID_CLIP_SEND, OnSendClipboardButton)
        COMMAND_ID_HANDLER(ID_CLIP_RECV, OnRecvClipboardButton)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAboutButton)
        COMMAND_ID_HANDLER(ID_EXIT, OnExitButton)
        COMMAND_ID_HANDLER(ID_AUTO_SIZE, OnAutoSizeButton)
        COMMAND_ID_HANDLER(ID_FULLSCREEN, OnFullScreenButton)
        COMMAND_ID_HANDLER(ID_POWER, OnDropDownButton)
        COMMAND_ID_HANDLER(ID_SHORTCUTS, OnDropDownButton)
        COMMAND_ID_HANDLER(ID_CAD, OnCADButton)

        COMMAND_RANGE_HANDLER(ID_KEY_FIRST, ID_KEY_LAST, OnKeyButton)
        COMMAND_RANGE_HANDLER(ID_POWER_FIRST, ID_POWER_LAST, OnPowerButton)

        NOTIFY_CODE_HANDLER(TBN_DROPDOWN, OnToolBarDropDown)
    END_MSG_MAP()

    ViewerWindow(CWindow *parent, Client *client, ClientConfig *config);
    ~ViewerWindow();

    void OnVideoUpdate(const uint8_t *buffer, const DesktopRegion &region);
    void OnVideoResize(const DesktopSize &size, const PixelFormat &format);

    void Worker() override
    {
        Create(nullptr);
        ShowWindow(SW_SHOW);

        CMessageLoop().Run();
    }

    void OnStop() override
    {
        PostMessageW(WM_CLOSE);
    }

private:
    int OnCreate(LPCREATESTRUCT create_struct);
    void OnDestroy();
    void OnClose();
    void OnSize(UINT type, CSize size);
    void OnGetMinMaxInfo(LPMINMAXINFO mmi);
    void OnActivate(UINT state, BOOL minimized, CWindow other_window);
    LRESULT OnKeyboard(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled);
    LRESULT OnSkipMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled);

    void OnSetFocus(CWindow old_window);
    void OnKillFocus(CWindow focus_window);

    LRESULT OnSettingsButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnBellButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnSendClipboardButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnRecvClipboardButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnAboutButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnExitButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnAutoSizeButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnFullScreenButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnDropDownButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);

    LRESULT OnPowerButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);

    LRESULT OnCADButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);
    LRESULT OnKeyButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled);

    LRESULT OnToolBarDropDown(int ctrl_id, LPNMHDR phdr, BOOL &handled);

    void ShowDropDownMenu(int button_id, RECT *button_rect);
    void DoAutoSize(const CSize &video_size, bool move);
    void DoFullScreen(BOOL fullscreen);

private:
    CWindow *parent_;
    Client *client_;
    ClientConfig *config_;
    ViewerBar toolbar_;
    VideoWindow video_window_;

    ScopedWindowsHook keyboard_hook_;

    WINDOWPLACEMENT window_pos_;
};

} // namespace aspia

#endif // _ASPIA_GUI__VIEWER_WINDOW_H
