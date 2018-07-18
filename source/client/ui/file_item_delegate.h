//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef _ASPIA_CLIENT__UI__FILE_ITEM_DELEGATE_H
#define _ASPIA_CLIENT__UI__FILE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

namespace aspia {

class FileReadOnlyColumnDelegate : public QStyledItemDelegate
{
public:
    explicit FileReadOnlyColumnDelegate(QWidget* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        // Nothing
    }

    QWidget* createEditor(QWidget* /* parent */,
                          const QStyleOptionViewItem& /* option */,
                          const QModelIndex& /* index */) const override
    {
        return nullptr;
    }

private:
    Q_DISABLE_COPY(FileReadOnlyColumnDelegate)
};

class FileColumnDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FileColumnDelegate(QWidget* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        // Nothing
    }

    void setModelData(QWidget* editor,
                      QAbstractItemModel* model,
                      const QModelIndex& index) const override
    {
        QStyledItemDelegate::setModelData(editor, model, index);

        if (index.column() == 0)
            emit editingFinished(index);
    }

signals:
    void editingFinished(const QModelIndex& index) const;

private:
    Q_DISABLE_COPY(FileColumnDelegate)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__FILE_ITEM_DELEGATE_H
