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

#include <optional>

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
      name_(new LineEdit()),
      comment_(new TextArea()),
      error_(new Label(QString(), Label::Role::CAPTION))
{
    name_->setLabel(tr("Name"));
    comment_->setLabel(tr("Comment"));

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    error_->setWordWrap(true);
    error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    delete_button_ = new Button(tr("Delete"), Button::Role::TEXT);
    delete_button_->setAccentColor(Controls::errorColor());
    delete_button_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(error_);
    form_layout->addWidget(name_);
    form_layout->addWidget(comment_);
    form_layout->addWidget(save);
    form_layout->addWidget(delete_button_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &LocalGroupEditor::onSaveClicked);
    connect(delete_button_, &Button::clicked, this, &LocalGroupEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
LocalGroupEditor::~LocalGroupEditor() = default;

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::prepareForAdd(qint64 parent_id)
{
    entry_id_ = -1;
    parent_id_ = parent_id;

    name_->clear();
    comment_->clear();
    error_->setVisible(false);
    delete_button_->hide();

    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::prepareForEdit(qint64 group_id)
{
    std::optional<GroupConfig> group = Database::instance().findGroup(group_id);
    if (!group.has_value())
        return;

    entry_id_ = group_id;
    parent_id_ = group->parentId();

    name_->setText(group->name());
    comment_->setText(group->comment());
    error_->setVisible(false);
    delete_button_->show();

    name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::onSaveClicked()
{
    const QString name = name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        name_->setFocus();
        return;
    }

    GroupConfig data;
    data.setId(entry_id_);
    data.setParentId(parent_id_);
    data.setName(name);
    data.setComment(comment_->text());

    Database& db = Database::instance();
    const bool saved = (entry_id_ < 0) ? db.addGroup(data) : db.modifyGroup(data);
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
                                tr("Delete the group \"%1\"?").arg(name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeGroup(entry_id_))
    {
        showError(tr("Failed to delete the group."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalGroupEditor::showError(const QString& message)
{
    error_->setText(message);
    error_->setVisible(true);
}
