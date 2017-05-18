//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/settings_dialog.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SETTINGS_DIALOG_H
#define _ASPIA_UI__SETTINGS_DIALOG_H

#include "proto/auth_session.pb.h"
#include "proto/desktop_session.pb.h"
#include "ui/base/modal_dialog.h"

namespace aspia {

class SettingsDialog : public ModalDialog
{
public:
    SettingsDialog() = default;

    INT_PTR DoModal(HWND parent,
                    proto::SessionType session_type,
                    const proto::DesktopSessionConfig& config);

    const proto::DesktopSessionConfig& Config() const { return config_; }

private:
    INT_PTR DoModal(HWND parent) override;
    INT_PTR OnMessage(UINT msg, WPARAM wparam, LPARAM lparam) override;

    void OnInitDialog();
    void OnHScroll(HWND ctrl);

    void OnCodecChanged(WORD notify_code);
    void OnOkButton();

    void InitCodecList();
    void InitColorDepthList();
    void UpdateCompressionRatio(int compression_ratio);
    void AddColorDepth(HWND combobox, int index, UINT string_id);
    void OnCodecChanged();

    proto::SessionType session_type_ = proto::SessionType::SESSION_TYPE_UNKNOWN;
    proto::DesktopSessionConfig config_;

    DISALLOW_COPY_AND_ASSIGN(SettingsDialog);
};

} // namespace aspia

#endif // _ASPIA_UI__SETTINGS_DIALOG_H
