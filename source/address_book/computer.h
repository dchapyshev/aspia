//
// PROJECT:         Aspia
// FILE:            address_book/computer.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_ADDRESS_BOOK__COMPUTER_H
#define _ASPIA_ADDRESS_BOOK__COMPUTER_H

#include <QtWidgets/QTreeWidget>

#include "base/macros.h"
#include "proto/address_book.pb.h"

class ComputerGroup;

class Computer : public QTreeWidgetItem
{
public:
    ~Computer();

    static std::unique_ptr<Computer> Create();

    QString Name() const;
    void SetName(const QString& name);
    QString Comment() const;
    void SetComment(const QString& comment);
    QString Address() const;
    void SetAddress(const QString& address);
    int Port() const;
    void SetPort(int port);
    QString UserName() const;
    void SetUserName(const QString& username);
    QString Password() const;
    void SetPassword(const QString& password);
    proto::desktop::Config DesktopManageSessionConfig() const;
    void SetDesktopManageSessionConfig(const proto::desktop::Config& config);
    proto::desktop::Config DesktopViewSessionConfig() const;
    void SetDesktopViewSessionConfig(const proto::desktop::Config& config);

    ComputerGroup* ParentComputerGroup();

private:
    friend class ComputerGroup;

    Computer(proto::Computer* computer, ComputerGroup* parent_group);

    proto::Computer* computer_;
    ComputerGroup* parent_group_;

    DISALLOW_COPY_AND_ASSIGN(Computer);
};

#endif // _ASPIA_ADDRESS_BOOK__COMPUTER_H
