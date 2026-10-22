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

#include "host/ui/domain_group_dialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/logging.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "host/domain_logon.h"
#include "host/host_domain_authenticator.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
DomainGroupDialog::DomainGroupDialog(
    const DomainGroupRule& rule, const QStringList& existing, QWidget* parent)
    : QDialog(parent),
      rule_(rule),
      existing_(existing)
{
    LOG(INFO) << "Ctor";

    setWindowTitle(tr("Domain Group"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label_group = new QLabel(tr("Group or user (name, SID, GID or uid:N):"), this);
    layout->addWidget(label_group);

    edit_group_ = new QLineEdit(this);
    edit_group_->setText(rule_.group);
    edit_group_->setMaxLength(256);
    edit_group_->setPlaceholderText(QStringLiteral("CORP\\Aspia Operators"));
    label_group->setBuddy(edit_group_);
    layout->addWidget(edit_group_);

    QLabel* label_hint = new QLabel(
        tr("Examples: \"CORP\\Aspia Operators\", \"S-1-5-21-...-1105\" (Windows SID), "
           "\"aspia-operators\" or \"1500\" (Linux group or GID), \"uid:1001\" (a single Linux "
           "user). On Linux a domain group or user can be written as \"DOMAIN\\name\", "
           "\"name@domain\" or \"name\" (any case); if a name is both a group and a user, add "
           "\"group:\" or \"user:\" in front, or enter the GID / \"uid:N\". The name is resolved to its SID / GID when you press OK, and access is checked by "
           "that identifier only. A name without a domain is looked up only on this computer and in "
           "the domain it is joined to; write a group of a trusted domain as \"DOMAIN\\Group\"."),
        this);
    label_hint->setWordWrap(true);
    layout->addWidget(label_hint);

    layout->addWidget(new QLabel(tr("Allowed sessions:"), this));

    tree_sessions_ = new QTreeWidget(this);
    tree_sessions_->setHeaderHidden(true);
    tree_sessions_->setRootIsDecorated(false);
    layout->addWidget(tree_sessions_);

    const proto::peer::SessionType kSessionTypes[] =
    {
        proto::peer::SESSION_TYPE_DESKTOP,
        proto::peer::SESSION_TYPE_TERMINAL,
        proto::peer::SESSION_TYPE_FILE_TRANSFER,
        proto::peer::SESSION_TYPE_SYSTEM_INFO,
        proto::peer::SESSION_TYPE_CHAT
    };

    for (proto::peer::SessionType session_type : kSessionTypes)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, sessionName(session_type));
        item->setIcon(0, sessionIcon(session_type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(static_cast<quint32>(session_type)));
        item->setCheckState(0, (rule_.sessions & static_cast<quint32>(session_type)) ?
            Qt::Checked : Qt::Unchecked);
        tree_sessions_->addTopLevelItem(item);
    }

    button_box_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(button_box_);

    connect(button_box_, &QDialogButtonBox::accepted, this, &DomainGroupDialog::onAccepted);
    connect(button_box_, &QDialogButtonBox::rejected, this, &DomainGroupDialog::reject);

    resize(420, 380);
}

//--------------------------------------------------------------------------------------------------
DomainGroupDialog::~DomainGroupDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DomainGroupDialog::onAccepted()
{
    const QString group = edit_group_->text().trimmed();
    if (group.isEmpty())
    {
        MsgBox::warning(this, tr("The group can not be empty."));
        edit_group_->setFocus();
        return;
    }

    for (const QString& other : existing_)
    {
        if (other.compare(group, Qt::CaseInsensitive) == 0)
        {
            MsgBox::warning(this, tr("This group is already in the list."));
            edit_group_->setFocus();
            edit_group_->selectAll();
            return;
        }
    }

    quint32 sessions = 0;
    for (int i = 0; i < tree_sessions_->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = tree_sessions_->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            sessions |= item->data(0, Qt::UserRole).toUInt();
    }

    if (!sessions)
    {
        MsgBox::warning(this, tr("Select at least one session type."));
        return;
    }

    // Resolve the name now: access is checked by the identifier only, so that a group or a user
    // with the same name in another domain never matches.
    DomainAccountInfo info;
    QString error;
    bool resolved;
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        resolved = resolveDomainAccount(group, &info, &error);
        QApplication::restoreOverrideCursor();
    }

    if (!resolved || info.id.isEmpty())
    {
        LOG(INFO) << "Unable to resolve domain group" << group << ":" << error;
        MsgBox::warning(this, tr("Unable to find the group or user \"%1\": %2").arg(group, error));
        edit_group_->setFocus();
        edit_group_->selectAll();
        return;
    }

    const QString display_name = info.name.isEmpty() ? group : info.name;

    if (info.kind == DomainGroupRule::Kind::GROUP &&
        HostDomainAuthenticator::isBroadGroup(info.id, display_name))
    {
        LOG(INFO) << "Broad group selected:" << display_name << "id:" << info.id;

        if (MsgBox::question(this,
                tr("The group \"%1\" contains practically every account of the computer or the "
                   "domain. Every member will be able to connect to this computer with the selected "
                   "sessions. It is recommended to create a separate group for Aspia users instead. "
                   "Do you really want to allow access to this group?").arg(display_name))
            != MsgBox::Yes)
        {
            edit_group_->setFocus();
            edit_group_->selectAll();
            return;
        }

        LOG(INFO) << "[ACTION] Broad group confirmed by user";
    }

    for (const QString& other : existing_)
    {
        if (other.compare(display_name, Qt::CaseInsensitive) == 0)
        {
            MsgBox::warning(this, tr("This group is already in the list."));
            edit_group_->setFocus();
            edit_group_->selectAll();
            return;
        }
    }

    rule_.group = display_name;
    rule_.id = info.id;
    rule_.kind = info.kind;
    rule_.sessions = sessions;

    LOG(INFO) << "[ACTION] Domain group accepted:" << rule_.group << "id:" << rule_.id
              << "sessions:" << sessions;
    accept();
}
