//
// PROJECT:         Aspia
// FILE:            ui/desktop/settings_dialog.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__DESKTOP__SETTINGS_DIALOG_H
#define _ASPIA_UI__DESKTOP__SETTINGS_DIALOG_H

#include "base/macros.h"
#include "proto/auth_session.pb.h"
#include "proto/desktop_session.pb.h"
#include "ui/resource.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

namespace aspia {

class SettingsDialog : public CDialogImpl<SettingsDialog>
{
public:
    enum { IDD = IDD_SETTINGS };

    SettingsDialog(proto::auth::SessionType session_type,const proto::SessionConfig& config);
    ~SettingsDialog() = default;

    const proto::SessionConfig& Config() const { return config_; }

private:
    BEGIN_MSG_MAP(SettingsDialog)
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

    proto::auth::SessionType session_type_;
    proto::SessionConfig config_;

    DISALLOW_COPY_AND_ASSIGN(SettingsDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__DESKTOP__SETTINGS_DIALOG_H
