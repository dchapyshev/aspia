//
// PROJECT:         Aspia
// FILE:            console/console_tab.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_tab.h"

namespace aspia {

ConsoleTab::ConsoleTab(Type type, QWidget* parent)
    : QWidget(parent), type_(type)
{
    // Nothing
}

} // namespace aspia
