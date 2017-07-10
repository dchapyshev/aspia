//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/settings_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SETTINGS_DIALOG_H
#define _ASPIA_UI__SETTINGS_DIALOG_H

#include "base/macros.h"
#include "proto/auth_session.pb.h"
#include "proto/desktop_session.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

namespace aspia {

class UiSettingsDialog : public CDialogImpl<UiSettingsDialog>
{
public:
    enum { IDD = IDD_SETTINGS };

    UiSettingsDialog(proto::SessionType session_type,
                     const proto::DesktopSessionConfig& config);
    ~UiSettingsDialog() = default;

    const proto::DesktopSessionConfig& Config() const { return config_; }

private:
    BEGIN_MSG_MAP(UiSettingsDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)

        COMMAND_ID_HANDLER(IDOK, OnOkButton)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancelButton)

        COMMAND_HANDLER(IDC_CODEC_COMBO, CBN_SELCHANGE, OnCodecListChanged)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnHScroll(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    LRESULT OnOkButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCancelButton(WORD notify_code, WORD control_id, HWND control, BOOL& handled);
    LRESULT OnCodecListChanged(WORD notify_code, WORD control_id, HWND control, BOOL& handled);

    void InitCodecList();
    void AddColorDepth(CComboBox& combobox, UINT string_id, int item_data);
    void InitColorDepthList();
    void OnCodecChanged();
    void UpdateCompressionRatio(int compression_ratio);
    void SelectItemWithData(CComboBox& combobox, int item_data);

    proto::SessionType session_type_;
    proto::DesktopSessionConfig config_;

    DISALLOW_COPY_AND_ASSIGN(UiSettingsDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__SETTINGS_DIALOG_H
