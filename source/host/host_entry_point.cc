//
// PROJECT:         Aspia
// FILE:            host/host_entry_point.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"

int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    LPWSTR /* lpCmdLine */,
                    int /* nCmdShow */)
{
    aspia::HostMain();
    return 0;
}
