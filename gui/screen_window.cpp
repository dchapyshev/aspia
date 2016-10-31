/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/screen_window.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "stdafx.h"
#include "screen_window.h"

LRESULT CScreenWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    CenterWindow();
    return 0;
}

LRESULT CScreenWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    return 0;
}

LRESULT CScreenWindow::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    DestroyWindow();
    return 0;
}

LRESULT CScreenWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    PostQuitMessage(0);
    return 0;
}

LRESULT CScreenWindow::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    return 0;
}
