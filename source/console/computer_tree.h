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

#ifndef ASPIA_CONSOLE__COMPUTER_TREE_H_
#define ASPIA_CONSOLE__COMPUTER_TREE_H_

#include <QTreeWidget>

#include "base/macros_magic.h"

namespace aspia {

class ComputerTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ComputerTree(QWidget* parent = nullptr);
    ~ComputerTree() = default;

protected:
    // QTreeWidget implementation.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supported_actions) override;

private:
    QPoint start_pos_;

    DISALLOW_COPY_AND_ASSIGN(ComputerTree);
};

} // namespace aspia

#endif // ASPIA_CONSOLE__COMPUTER_TREE_H_
