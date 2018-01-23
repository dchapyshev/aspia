//
// PROJECT:         Aspia
// FILE:            ui/ui_main.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__UI_MAIN_H
#define _ASPIA_UI__UI_MAIN_H

namespace aspia {

enum class UI { MAIN_DIALOG, ADDRESS_BOOK };

void RunUIMain(UI ui);

} // namespace aspia

#endif // _ASPIA_UI__UI_MAIN_H
