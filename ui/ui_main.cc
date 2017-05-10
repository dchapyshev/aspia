//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/ui_main.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/ui_main.h"
#include "ui/main_dialog.h"
#include "base/message_loop/message_loop.h"
#include "base/scoped_com_initializer.h"
#include "network/scoped_wsa_initializer.h"

#include <commctrl.h>

namespace aspia {

void RunUIMain()
{
    ScopedCOMInitializer com_initializer;
    ScopedWsaInitializer wsa_initializer;

    CHECK(com_initializer.IsSucceeded());
    CHECK(wsa_initializer.IsSucceeded());

    INITCOMMONCONTROLSEX iccx;

    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_BAR_CLASSES;

    InitCommonControlsEx(&iccx);

    MainDialog main_dialog;
    main_dialog.DoModal(nullptr);
}

} // namespace aspia
