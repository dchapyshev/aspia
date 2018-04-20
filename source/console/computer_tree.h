//
// PROJECT:         Aspia
// FILE:            console/computer_tree.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CONSOLE__COMPUTER_TREE_H
#define _ASPIA_CONSOLE__COMPUTER_TREE_H

#include <QTreeWidget>

namespace aspia {

class ComputerTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ComputerTree(QWidget* parent = nullptr);
    ~ComputerTree();

private:
    Q_DISABLE_COPY(ComputerTree)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_TREE_H
