/*
* PROJECT:         Aspia Remote Desktop
* FILE:            gui/main.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include <atlbase.h>
#include <atlapp.h>

#include "gui/main_dialog.h"

int main(int argc, char *argv[])
{
    // Парсим параметры коммандной строки.
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // Инициализируем библиотеку логгирования.
    google::InitGoogleLogging(argv[0]);

    HINSTANCE instance = nullptr;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<WCHAR*>(&main),
                       &instance);
    CHECK(instance);

    HRESULT res = CoInitialize(nullptr);
    CHECK(SUCCEEDED(res));

    AtlInitCommonControls(ICC_BAR_CLASSES);

    CAppModule module;
    res = module.Init(nullptr, instance);
    CHECK(SUCCEEDED(res));

    CMainDialog main_dialog;
    main_dialog.DoModal();

    module.Term();

    CoUninitialize();

    google::ShutdownGoogleLogging();
    gflags::ShutDownCommandLineFlags();

    return 0;
}
