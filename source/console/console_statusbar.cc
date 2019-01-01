//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
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
        QString("<table><tr><td><img src=':/img/folder.png'></td><td>%1</td></tr></table>")
        .arg(QString::fromStdString(computer_group.name())), this);

    QLabel* second_label = new QLabel(
        QString("<table><tr><td><img src=':/img/folder.png'></td><td>%1</td></tr></table>")
        .arg(child_groups), this);

    QLabel* third_label = new QLabel(
        QString("<table><tr><td><img src=':/img/computer.png'></td><td>%1</td></tr></table>")
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
