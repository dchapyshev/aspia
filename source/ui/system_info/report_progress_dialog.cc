//
// PROJECT:         Aspia
// FILE:            ui/system_info/report_progress_dialog.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "ui/system_info/report_progress_dialog.h"

namespace aspia {

ReportProgressDialog::ReportProgressDialog(
    CategoryList* list, Report* report, ReportCreator::RequestCallback request_callback)
    : report_creator_(std::make_unique<ReportCreator>(list, report, request_callback))
{
    // Nothing
}

LRESULT ReportProgressDialog::OnInitDialog(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    CenterWindow();

    report_creator_->Start(
        std::bind(&ReportProgressDialog::OnStateChanged, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&ReportProgressDialog::OnTerminate, this));

    return FALSE;
}

LRESULT ReportProgressDialog::OnClose(
    UINT /* message */, WPARAM /* wparam */, LPARAM /* lparam */, BOOL& /* handled */)
{
    report_creator_.reset();
    EndDialog(0);
    return 0;
}

LRESULT ReportProgressDialog::OnCancelButton(
    WORD /* notify_code */, WORD /* ctrl_id */, HWND /* ctrl */, BOOL& /* handled */)
{
    SendMessageW(WM_CLOSE);
    return 0;
}

void ReportProgressDialog::OnStateChanged(std::string_view category_name,
                                          ReportCreator::State state)
{
    GetDlgItem(IDC_CURRENT_CATEGORY).SetWindowTextW(
        UNICODEfromUTF8(category_name.data()).c_str());

    CString state_string;

    switch (state)
    {
        case ReportCreator::State::REQUEST:
            state_string.LoadStringW(IDS_SI_STATE_REQUEST);
            break;

        case ReportCreator::State::OUTPUT:
            state_string.LoadStringW(IDS_SI_STATE_OUTPUT);
            break;

        default:
            DLOG(LS_ERROR) << "Unhandled state: " << static_cast<int>(state);
            return;
    }

    GetDlgItem(IDC_CURRENT_ACTION).SetWindowTextW(state_string);
}

void ReportProgressDialog::OnTerminate()
{
    PostMessageW(WM_CLOSE);
}

} // namespace aspia
