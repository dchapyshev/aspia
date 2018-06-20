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

    Q_DISABLE_COPY(ComputerTree)
};

} // namespace aspia

#endif // _ASPIA_CONSOLE__COMPUTER_TREE_H
