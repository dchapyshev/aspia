//
// PROJECT:         Aspia
// FILE:            ui/ui_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_main.h"
#include "ui/main_dialog.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_dispatcher.h"
#include "base/scoped_com_initializer.h"

#include <atlbase.h>
#include <atlapp.h>

#include <atlwin.h>
#include <commctrl.h>

namespace aspia {

void RunUIMain()
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
        LOG(LS_ERROR) << "GetModuleHandleExW() failed: " << GetLastSystemErrorString();
        return;
    }

    CAppModule module;
    HRESULT hr = module.Init(nullptr, instance);
    if (FAILED(hr))
    {
        LOG(LS_ERROR) << "Module initialization failure: " << SystemErrorCodeToString(hr);
        return;
    }

    MainDialog main_dialog;
    if (!main_dialog.Create(nullptr, 0))
    {
        LOG(LS_ERROR) << "Unable to create main dialog: " << GetLastSystemErrorString();
    }
    else
    {
        main_dialog.ShowWindow(SW_SHOWNORMAL);
        main_dialog.UpdateWindow();

        MessageLoopForUI message_loop;
        MessageDispatcherForDialog dispatcher(main_dialog);
        message_loop.Run(&dispatcher);
    }

    module.Term();
}

} // namespace aspia
