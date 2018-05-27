//
// PROJECT:         Aspia
// FILE:            client/ui/file_item_delegate.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
