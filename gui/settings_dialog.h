//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/settings_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__SETTINGS_DIALOG_H
#define _ASPIA_GUI__SETTINGS_DIALOG_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <atlcrack.h>
#include <atlctrls.h>

#include <vector>
#include <memory>

#include "client/client_config.h"
#include "gui/resource.h"

namespace aspia {

class SettingsDialog : public CDialogImpl<SettingsDialog>
{
public:
    enum { IDD = IDD_SETTINGS };

    const ScreenConfig& GetConfig() const
    {
        return config_;
    }

private:
    BEGIN_MSG_MAP(SettingsDialog)
        MSG_WM_INITDIALOG(OnInitDialog)
        MSG_WM_CLOSE(OnClose)
        MSG_WM_HSCROLL(OnHScroll)

        COMMAND_ID_HANDLER(IDC_CODEC_COMBO, OnCodecChanged)
        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)
    END_MSG_MAP()

    BOOL OnInitDialog(CWindow focus_window, LPARAM lParam);
    void OnClose();
    void OnHScroll(UINT code, UINT pos, HWND ctrl);

    LRESULT OnCodecChanged(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnOkButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL& handled);

    void InitCodecList();
    void InitColorDepthList();
    void AddColorDepth(CComboBox& combobox, int index, UINT string_id);
    void DoCodecChanged();

private:
    static const int kVP9  = 0;
    static const int kVP8  = 1;
    static const int kZLIB = 2;

    static const int kARGB   = 0;
    static const int kRGB888 = 1;
    static const int kRGB565 = 2;
    static const int kRGB555 = 3;
    static const int kRGB444 = 4;
    static const int kRGB332 = 5;
    static const int kRGB222 = 6;
    static const int kRGB111 = 7;

    ScreenConfig config_;
};

} // namespace aspia

#endif // _ASPIA_GUI__SETTINGS_DIALOG_H
