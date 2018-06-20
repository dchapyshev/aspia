//
// PROJECT:         Aspia
// FILE:            console/console_statusbar.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_statusbar.h"

#include <QIcon>
#include <QLabel>

namespace aspia {

ConsoleStatusBar::ConsoleStatusBar(QWidget* parent)
    : QStatusBar(parent)
{
    // Nothing
}

void ConsoleStatusBar::setCurrentComputerGroup(
    const proto::address_book::ComputerGroup& computer_group)
{
    clear();

    QString child_groups = tr("%n child group(s)", "", computer_group.computer_group_size());
    QString child_computers = tr("%n child computer(s)", "", computer_group.computer_size());

    QLabel* first_label = new QLabel(
        QString("<table><tr><td><img src=':/icon/folder.png'></td><td>%1</td></tr></table>")
        .arg(QString::fromStdString(computer_group.name())), this);

    QLabel* second_label = new QLabel(
        QString("<table><tr><td><img src=':/icon/folder.png'></td><td>%1</td></tr></table>")
        .arg(child_groups), this);

    QLabel* third_label = new QLabel(
        QString("<table><tr><td><img src=':/icon/computer.png'></td><td>%1</td></tr></table>")
        .arg(child_computers), this);

    first_label->setTextFormat(Qt::RichText);
    second_label->setTextFormat(Qt::RichText);
    third_label->setTextFormat(Qt::RichText);

    addWidget(first_label);
    addWidget(second_label);
    addWidget(third_label);
}

void ConsoleStatusBar::clear()
{
    QList<QLabel*> items = findChildren<QLabel*>();

    for (const auto& item : items)
    {
        removeWidget(item);
        delete item;
    }
}

} // namespace aspia
