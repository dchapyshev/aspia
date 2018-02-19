//
// PROJECT:         Aspia
// FILE:            ui/address_book_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/address_book/address_book_main.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <commctrl.h>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_com_initializer.h"
#include "base/settings_manager.h"
#include "base/logging.h"
#include "ui/address_book/address_book_window.h"

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
                            reinterpret_cast<wchar_t*>(&InitUIModule),
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

void AddressBookMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    CAppModule module;
    if (InitUIModule(module))
    {
        std::experimental::filesystem::path address_book_path;

        CommandLine::StringVector args = CommandLine::ForCurrentProcess().GetArgs();
        if (!args.empty())
            address_book_path = args.front();

        AddressBookWindow address_book_window(address_book_path);

        if (!address_book_window.Create(nullptr))
        {
            PLOG(LS_ERROR) << "Unable to create address book window";
        }
        else
        {
            address_book_window.ShowWindow(SW_SHOW);
            address_book_window.UpdateWindow();

            MessageLoopForUI message_loop;
            message_loop.Run(&address_book_window);
        }

        module.Term();
    }

    ShutdownLogging();
}

} // namespace aspia
