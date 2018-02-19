//
// PROJECT:         Aspia
// FILE:            ui/ui_main_entry_point.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_main.h"

int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    LPWSTR /* lpCmdLine */,
                    int /* nCmdShow */)
{
    aspia::UIMain();
    return 0;
}
