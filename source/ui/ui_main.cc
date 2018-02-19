//
// PROJECT:         Aspia
// FILE:            ui/ui_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_main.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <commctrl.h>

#include "base/command_line.h"
#include "base/files/base_paths.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_helpers.h"
#include "base/scoped_com_initializer.h"
#include "base/settings_manager.h"
#include "base/version_helpers.h"
#include "ui/address_book/address_book_window.h"
#include "ui/main_dialog.h"

namespace aspia {

namespace {

bool InitUIModule(CAppModule& module)
{
    ScopedCOMInitializer com_initializer;
    if (!com_initializer.IsSucceeded())
        return false;

    AtlInitCommonControls(ICC_BAR_CLASSES);

    HINSTANCE instance = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<WCHAR*>(&InitUIModule),
                            &instance))
    {
        PLOG(LS_ERROR) << "GetModuleHandleExW failed";
        return false;
    }

    HRESULT hr = module.Init(nullptr, instance);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "Module initialization failure: " << SystemErrorCodeToString(hr);
        return false;
    }

    SetThreadUILanguage(SettingsManager().GetUILanguage());
    return true;
}

} // namespace

void UIMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    if (IsWindowsVistaOrGreater() && !IsProcessElevated())
    {
        std::experimental::filesystem::path program_path;

        if (GetBasePath(BasePathKey::FILE_EXE, program_path))
        {
            if (LaunchProcessWithElevate(program_path))
            {
                ShutdownLogging();
                return;
            }
        }
    }

    CAppModule module;

    if (InitUIModule(module))
    {
        static const WCHAR kMutexName[] = L"aspia.mutex.main_dialog";

        ScopedHandle mutex(CreateMutexW(nullptr, FALSE, kMutexName));
        if (!mutex.IsValid() || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            DLOG(LS_WARNING) << "The application is already running.";
        }
        else
        {
            MainDialog main_dialog;

            if (!main_dialog.Create(nullptr, 0))
            {
                PLOG(LS_ERROR) << "Unable to create main dialog";
            }
            else
            {
                main_dialog.ShowWindow(SW_SHOWNORMAL);
                main_dialog.UpdateWindow();

                MessageLoopForUI message_loop;
                message_loop.Run(&main_dialog);
            }
        }

        module.Term();
    }

    ShutdownLogging();
}

} // namespace aspia
