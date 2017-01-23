//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/status_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/status_dialog.h"

#include <future>

#include "base/exception.h"
#include "base/unicode.h"

namespace aspia {

extern CIcon _small_icon;
extern CIcon _big_icon;

BOOL StatusDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    SetIcon(_small_icon, FALSE);
    SetIcon(_big_icon, TRUE);

    CenterWindow();

    CString message;
    message.Format(IDS_STATUS_CONNECTING_FORMAT,
                   UNICODEfromUTF8(config_.RemoteAddress()).c_str(),
                   config_.RemotePort());
    AddMessage(message);

    ClientTCP::OnConnectedCallback on_connected_callback =
        std::bind(&StatusDialog::OnConnected, this, std::placeholders::_1);

    ClientTCP::OnErrorCallback on_error_callback =
        std::bind(&StatusDialog::OnError, this, std::placeholders::_1);

    tcp_.reset(new ClientTCP(config_.RemoteAddress(),
                             config_.RemotePort(),
                             on_connected_callback,
                             on_error_callback));

    GetDlgItem(IDCANCEL).SetFocus();

    return FALSE;
}

void StatusDialog::OnClose()
{
    viewer_.reset();
    client_.reset();
    tcp_.reset();

    EndDialog(0);
}

LRESULT StatusDialog::OnAppMessage(UINT msg, WPARAM wParam, LPARAM lParam, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT StatusDialog::OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void StatusDialog::OnClientEvent(Client::Event type)
{
    if (type == Client::Event::NotConnected)
    {
        //Stop();
    }
    else if (type == Client::Event::Disconnected)
    {
        viewer_.reset();
        client_.reset();

        ShowWindow(SW_SHOW);
    }
    else if (type == Client::Event::AuthRequest)
    {

    }
    else if (type == Client::Event::BadAuth)
    {
        //Stop();
    }
    else if (type == Client::Event::Connected)
    {
        AddMessage(IDS_STATUS_CONNECTED);
        ShowWindow(SW_HIDE);

        viewer_.reset(new ViewerWindow(this, client_.get(), &config_));

        Client::OnVideoUpdateCallback on_video_update_callback =
            std::bind(&ViewerWindow::OnVideoUpdate, viewer_.get(), std::placeholders::_1, std::placeholders::_2);

        Client::OnVideoResizeCallback on_video_resize_callback =
            std::bind(&ViewerWindow::OnVideoResize, viewer_.get(), std::placeholders::_1, std::placeholders::_2);

        Client::OnCursorUpdateCallback on_cursor_update_callback =
            std::bind(&ViewerWindow::OnCursorUpdate, viewer_.get(), std::placeholders::_1);

        client_->StartScreenUpdate(config_.screen_config(),
                                   on_video_update_callback,
                                   on_video_resize_callback,
                                   on_cursor_update_callback);
    }
}

void StatusDialog::OnConnected(std::unique_ptr<Socket> &sock)
{
    try
    {
        Client::OnEventCallback on_event_callback =
            std::bind(&StatusDialog::OnClientEvent, this, std::placeholders::_1);

        client_.reset(new Client(std::move(sock), on_event_callback));
        tcp_.reset();
    }
    catch (const Exception &err)
    {
        AddMessage(UNICODEfromUTF8(err.What()).c_str());
    }
}

void StatusDialog::OnError(const char *reason)
{
    AddMessage(UNICODEfromUTF8(reason).c_str());
}

void StatusDialog::AddMessage(const WCHAR *message)
{
    CEdit status = GetDlgItem(IDC_STATUS_EDIT);

    int len = GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, nullptr, 0);
    if (len)
    {
        std::wstring date;

        date.resize(len + 1);

        if (GetDateFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, &date[0], len))
        {
            status.AppendText(date.c_str());
            status.AppendText(L" ");
        }
    }

    len = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, nullptr, 0);
    if (len)
    {
        std::wstring time;

        time.resize(len + 1);

        if (GetTimeFormatW(LOCALE_USER_DEFAULT, 0, nullptr, nullptr, &time[0], len))
        {
            status.AppendText(time.c_str());
            status.AppendText(L": ");
        }
    }

    status.AppendText(message);
    status.AppendText(L"\r\n");

    status.SetSelNone();
}

void StatusDialog::AddMessage(UINT message_id)
{
    CString message;
    message.LoadStringW(message_id);

    AddMessage(message);
}

} // namespace aspia
