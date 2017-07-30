//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/power_session_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/power_session_dialog.h"
#include "base/logging.h"

#include <atlmisc.h>

namespace aspia {

static const UINT_PTR kTimerId = 100;
static const int kTimeout = 60; // 60 seconds

UiPowerSessionDialog::UiPowerSessionDialog(proto::PowerEvent::Action action)
    : action_(action),
      timer_(kTimerId),
      time_left_(kTimeout)
{
    // Nothing
}

LRESULT UiPowerSessionDialog::OnInitDialog(UINT message, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled)
{
    CenterWindow();

    CString title;

    switch (action_)
    {
        case proto::PowerEvent::SHUTDOWN:
            title.LoadStringW(IDS_PM_SHUTDOWN_COMMAND);
            break;

        case proto::PowerEvent::REBOOT:
            title.LoadStringW(IDS_PM_REBOOT_COMMAND);
            break;

        case proto::PowerEvent::HIBERNATE:
            title.LoadStringW(IDS_PM_HIBERNATE_COMMAND);
            break;

        case proto::PowerEvent::SUSPEND:
            title.LoadStringW(IDS_PM_SUSPEND_COMMAND);
            break;

        default:
            DLOG(FATAL) << "Wrong power action: " << action_;
            break;
    }

    GetDlgItem(IDC_POWER_ACTION).SetWindowTextW(title);
    UpdateTimer();

    timer_.Start(*this, 1000);
    return TRUE;
}

LRESULT UiPowerSessionDialog::OnClose(UINT message, WPARAM wparam,
                                      LPARAM lparam, BOOL& handled)
{
    Exit(Result::CANCEL);
    return 0;
}

LRESULT UiPowerSessionDialog::OnTimer(UINT message, WPARAM wparam,
                                      LPARAM lparam, BOOL& handled)
{
    UINT_PTR event_id = static_cast<UINT_PTR>(wparam);

    if (event_id != kTimerId)
        return 0;

    --time_left_;

    if (time_left_ <= 0)
    {
        Exit(Result::EXECUTE);
    }
    else
    {
        UpdateTimer();
    }

    return 0;
}

LRESULT UiPowerSessionDialog::OnOkButton(WORD notify_code, WORD control_id,
                                         HWND control, BOOL& handled)
{
    Exit(Result::EXECUTE);
    return 0;
}

LRESULT UiPowerSessionDialog::OnCancelButton(WORD notify_code, WORD control_id,
                                             HWND control, BOOL& handled)
{
    Exit(Result::CANCEL);
    return 0;
}

void UiPowerSessionDialog::UpdateTimer()
{
    CString string;
    string.Format(IDS_PM_TIME_LEFT, time_left_);
    GetDlgItem(IDC_POWER_TIME).SetWindowTextW(string);
}

void UiPowerSessionDialog::Exit(Result result)
{
    timer_.Stop();
    EndDialog(static_cast<int>(result));
}

} // namespace aspia
