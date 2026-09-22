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

#include "client/android/local_group_editor.h"

#include <QVBoxLayout>

#include "base/logging.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/text_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalGroupEditor::LocalGroupEditor(QWidget* parent)
    : QWidget(parent),
      edit_name_(new LineEdit()),
      edit_comment_(new TextArea()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_name_->setLabel(tr("Name"));
    edit_comment_->setLabel(tr("Comment"));

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    button_delete_ = new Button(tr("Delete"), Button::Role::TEXT);
    button_delete_->setAccentColor(Controls::errorColor());
    button_delete_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_name_);
    form_layout->addWidget(edit_comment_);
    form_layout->addWidget(save);
    form_layout->addWidget(button_delete_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &LocalGroupEditor::onSaveClicked);
    connect(button_delete_, &Button::clicked, this, &LocalGroupEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
LocalGroupEditor::~LocalGroupEditor() = default;

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::prepareForAdd(qint64 parent_id)
{
    entry_id_ = -1;
    parent_id_ = parent_id;

    edit_name_->clear();
    edit_comment_->clear();
    label_error_->setVisible(false);
    button_delete_->hide();

    edit_name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool LocalGroupEditor::prepareForEdit(qint64 group_id)
{
    LocalGroupConfig group;
    if (Database::instance().findLocalGroup(group_id, &group) != Database::FindResult::FOUND)
    {
        LOG(ERROR) << "Group not found:" << group_id;
        return false;
    }

    entry_id_ = group_id;
    parent_id_ = group.parentId();

    edit_name_->setText(group.name());
    edit_comment_->setText(group.comment());
    label_error_->setVisible(false);
    button_delete_->show();

    edit_name_->setFocus();
    return true;
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::onSaveClicked()
{
    const QString name = edit_name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        edit_name_->setFocus();
        return;
    }

    if (name.length() > LocalGroupConfig::kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", LocalGroupConfig::kMaxNameLength));
        edit_name_->setFocus();
        edit_name_->selectAll();
        return;
    }

    if (edit_comment_->text().length() > LocalGroupConfig::kMaxCommentLength)
    {
        showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                     "", LocalGroupConfig::kMaxCommentLength));
        edit_comment_->setFocus();
        return;
    }

    LocalGroupConfig data;
    data.setId(entry_id_);
    data.setParentId(parent_id_);
    data.setName(name);
    data.setComment(edit_comment_->text());

    Database& db = Database::instance();
    const bool saved = (entry_id_ < 0) ? db.addLocalGroup(data) : db.modifyLocalGroup(data);
    if (!saved)
    {
        showError(tr("Failed to save the group."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Group"),
                                tr("Delete the group \"%1\"?").arg(edit_name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeLocalGroup(entry_id_))
    {
        showError(tr("Failed to delete the group."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
