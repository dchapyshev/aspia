//
// PROJECT:         Aspia
// FILE:            console/console_statusbar.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__CONSOLE_STATUSBAR_H
#define _ASPIA_CONSOLE__CONSOLE_STATUSBAR_H

#include <QLabel>
#include <QStatusBar>

#include "protocol/address_book.pb.h"

namespace aspia {

class ConsoleStatusBar : public QStatusBar
{
    Q_OBJECT

public:
    explicit ConsoleStatusBar(QWidget* parent);
    ~ConsoleStatusBar() = default;

    void setCurrentComputerGroup(const proto::address_book::ComputerGroup& computer_group);
    void clear();

private:
    QVector<QLabel*> item_list_;

    Q_DISABLE_COPY(ConsoleStatusBar)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__CONSOLE_STATUSBAR_H
