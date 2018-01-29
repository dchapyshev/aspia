//
// PROJECT:         Aspia
// FILE:            ui/ui_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_main.h"
#include "ui/main_dialog.h"
#include "ui/address_book/address_book_window.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_com_initializer.h"

#include <atlbase.h>
#include <atlapp.h>

#include <atlwin.h>
#include <commctrl.h>

namespace aspia {

void RunUIMain(UI ui)
{
    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

    AtlInitCommonControls(ICC_BAR_CLASSES);

    HINSTANCE instance = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<WCHAR*>(&RunUIMain),
                            &instance))
    {
        PLOG(LS_ERROR) << "GetModuleHandleExW failed";
        return;
    }

    CAppModule module;
    HRESULT hr = module.Init(nullptr, instance);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "Module initialization failure: " << SystemErrorCodeToString(hr);
        return;
    }

    if (ui == UI::MAIN_DIALOG)
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
    else
    {
        DCHECK_EQ(ui, UI::ADDRESS_BOOK);

        AddressBookWindow address_book_window;

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
    }

    module.Term();
}

} // namespace aspia
