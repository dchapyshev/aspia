//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_transfer/file_item_delegate.h"

#include "client/ui/file_transfer/file_name_validator.h"
#include "common/file_platform_util.h"

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QLineEdit>
#include <QToolTip>

namespace client {

namespace {

class Editor final : public QLineEdit
{
public:
    explicit Editor(QWidget* parent)
        : QLineEdit(parent)
    {
        FileNameValidator* validator = new FileNameValidator(this);

        connect(validator, &FileNameValidator::sig_invalidNameEntered, this, [this]()
        {
            QString characters;

            for (const auto& character : common::FilePlatformUtil::invalidFileNameCharacters())
            {
                if (!characters.isEmpty())
                    characters += QLatin1String(", ");

                characters += character;
            }

            QString message =
                QApplication::translate("FileNameEditor", "The name can not contain characters %1.")
                .arg(characters);

            QToolTip::showText(mapToGlobal(QPoint(0, 0)), message, this);
        });

        setValidator(validator);
    }

    void setIcon(const QIcon& icon)
    {
        pixmap_ = icon.pixmap(QSize(16, 16));
        if (!pixmap_.isNull())
            setTextMargins(20, 2, 2, 2);
    }

protected:
    void paintEvent(QPaintEvent* event) final
    {
        QLineEdit::paintEvent(event);

        if (!pixmap_.isNull())
        {
            QPainter painter(this);

            QPoint pos(2, (height() / 2) - (pixmap_.height() / 2));
            painter.drawPixmap(pos, pixmap_);
        }
    }

private:
    QPixmap pixmap_;
    Q_DISABLE_COPY(Editor)
};

} // namespace

//--------------------------------------------------------------------------------------------------
FileItemDelegate::FileItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    connect(this, &FileItemDelegate::closeEditor, this, &FileItemDelegate::sig_editFinished);
}

//--------------------------------------------------------------------------------------------------
QWidget* FileItemDelegate::createEditor(QWidget* parent,
                                        const QStyleOptionViewItem& /* option */,
                                        const QModelIndex& index) const
{
    Editor* editor = new Editor(parent);
    editor->setIcon(index.data(Qt::DecorationRole).value<QIcon>());
    return editor;
}

//--------------------------------------------------------------------------------------------------
void FileItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    Editor* edit = dynamic_cast<Editor*>(editor);
    if (!edit)
        return;

    edit->setText(index.data(Qt::EditRole).toString());
}

//--------------------------------------------------------------------------------------------------
void FileItemDelegate::setModelData(QWidget* editor,
                                    QAbstractItemModel* model,
                                    const QModelIndex& index) const
{
    Editor* edit = dynamic_cast<Editor*>(editor);
    if (!edit)
        return;

    model->setData(index, edit->text(), Qt::EditRole);
}

//--------------------------------------------------------------------------------------------------
void FileItemDelegate::updateEditorGeometry(QWidget* editor,
                                            const QStyleOptionViewItem& option,
                                            const QModelIndex& /* index */) const
{
    Editor* edit = dynamic_cast<Editor*>(editor);
    if (!edit)
        return;

    edit->setGeometry(option.rect);
}

} // namespace client
