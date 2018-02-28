//
// PROJECT:         Aspia
// FILE:            address_book/computer_group.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_H

#include <QtWidgets/QTreeWidget>

#include "address_book/computer.h"
#include "proto/address_book.pb.h"

class ComputerGroup : public QTreeWidgetItem
{
public:
    virtual ~ComputerGroup();

    static std::unique_ptr<ComputerGroup> Create();

    void AddChildComputerGroup(ComputerGroup* computer_group);
    bool DeleteChildComputerGroup(ComputerGroup* computer_group);
    ComputerGroup* TakeChildComputerGroup(ComputerGroup* computer_group);
    void AddChildComputer(Computer* computer);
    bool DeleteChildComputer(Computer* computer);
    QString Name() const;
    void SetName(const QString& name);
    QString Comment() const;
    void SetComment(const QString& name);
    bool IsExpanded() const;
    void SetExpanded(bool expanded);
    QList<QTreeWidgetItem*> ComputerList();
    ComputerGroup* ParentComputerGroup();

protected:
    ComputerGroup(const QIcon& icon,
                  proto::ComputerGroup* group,
                  ComputerGroup* parent_group);

private:
    friend class ComputerGroupTree;
    friend class AddressBook;

    proto::ComputerGroup* group_;
    ComputerGroup* parent_group_;

    DISALLOW_COPY_AND_ASSIGN(ComputerGroup);
};

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_GROUP_H
