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

#ifndef CLIENT_UI_HOSTS_LOCAL_GROUP_WIDGET_H
#define CLIENT_UI_HOSTS_LOCAL_GROUP_WIDGET_H

#include <QDrag>
#include <QMimeData>

#include "client/ui/hosts/content_widget.h"
#include "ui_local_group_widget.h"

namespace client {

struct ComputerData;

class LocalGroupWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit LocalGroupWidget(QWidget* parent = nullptr);
    ~LocalGroupWidget() override;

    class Item : public QTreeWidgetItem
    {
    public:
        Item(const ComputerData& computer, QTreeWidget* parent);

        qint64 computerId() const { return computer_id_; }
        qint64 groupId() const { return group_id_; }
        QString computerName() const { return computer_name_; }

    private:
        qint64 computer_id_;
        qint64 group_id_;
        QString computer_name_;
    };

    Item* currentItem();
    void showGroup(qint64 group_id);
    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

    class ComputerMimeData final : public QMimeData
    {
    public:
        ComputerMimeData() = default;
        virtual ~ComputerMimeData() final = default;

        void setComputerItem(Item* computer_item, const QString& mime_type)
        {
            computer_item_ = computer_item;
            setData(mime_type, QByteArray());
        }

        Item* computerItem() const { return computer_item_; }

    private:
        Item* computer_item_ = nullptr;
    };

    class ComputerDrag final : public QDrag
    {
    public:
        explicit ComputerDrag(QObject* drag_source = nullptr)
            : QDrag(drag_source)
        {
            // Nothing
        }

        void setComputerItem(Item* computer_item, const QString& mime_type)
        {
            ComputerMimeData* mime_data = new ComputerMimeData();
            mime_data->setComputerItem(computer_item, mime_type);
            setMimeData(mime_data);
        }
    };

signals:
    void sig_doubleClicked(qint64 computer_id);
    void sig_currentChanged(qint64 computer_id);
    void sig_contextMenu(qint64 computer_id, const QPoint& pos);

private slots:
    void onHeaderContextMenu(const QPoint& pos);

private:
    Ui::LocalGroupWidget ui;

    Q_DISABLE_COPY_MOVE(LocalGroupWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_LOCAL_GROUP_WIDGET_H
