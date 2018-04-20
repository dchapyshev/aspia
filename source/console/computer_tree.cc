//
// PROJECT:         Aspia
// FILE:            console/computer_tree.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_tree.h"

namespace aspia {

ComputerTree::ComputerTree(QWidget* parent)
    : QTreeWidget(parent)
{
    // Nothing
}

ComputerTree::~ComputerTree() = default;

} // namespace aspia
