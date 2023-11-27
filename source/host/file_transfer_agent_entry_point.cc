//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "build/build_config.h"
#include "host/file_transfer_agent_main.h"

#if defined(OS_WIN)
#include <Windows.h>

//--------------------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE /* hInstance */,
                    HINSTANCE /* hPrevInstance */,
                    LPWSTR /* lpCmdLine */,
                    int /* nCmdShow */)
{
    fileTransferAgentMain(0, nullptr); // On Windows ignores arguments.
    return 0;
}

#else

//--------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    fileTransferAgentMain(argc, argv);
    return 0;
}

#endif
