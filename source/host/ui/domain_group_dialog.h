//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_UI_DOMAIN_GROUP_DIALOG_H
#define HOST_UI_DOMAIN_GROUP_DIALOG_H

#include <QDialog>

#include "host/domain_group_rule.h"

class QDialogButtonBox;
class QLineEdit;
class QTreeWidget;

// Edits one operating system / domain group and the session types its members get.
class DomainGroupDialog final : public QDialog
{
    Q_OBJECT

public:
    // |existing| is the list of the other groups, used to refuse duplicates.
    DomainGroupDialog(const DomainGroupRule& rule, const QStringList& existing, QWidget* parent = nullptr);
    ~DomainGroupDialog() final;

    const DomainGroupRule& rule() const { return rule_; }

private slots:
    void onAccepted();

private:
    DomainGroupRule rule_;
    const QStringList existing_;

    QLineEdit* edit_group_ = nullptr;
    QTreeWidget* tree_sessions_ = nullptr;
    QDialogButtonBox* button_box_ = nullptr;

    Q_DISABLE_COPY_MOVE(DomainGroupDialog)
};

#endif // HOST_UI_DOMAIN_GROUP_DIALOG_H
