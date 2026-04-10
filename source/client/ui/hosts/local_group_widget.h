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

#include "client/ui/hosts/content_tree_item.h"
#include "client/ui/hosts/content_widget.h"
#include "ui_local_group_widget.h"

namespace client {

class LocalGroupWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit LocalGroupWidget(QWidget* parent = nullptr);
    ~LocalGroupWidget() override;

    LocalComputerItem* currentComputer();
    void showGroup(qint64 group_id);
    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

    class ComputerMimeData final : public QMimeData
    {
    public:
        ComputerMimeData() = default;
        virtual ~ComputerMimeData() final = default;

        void setComputerItem(LocalComputerItem* computer_item, const QString& mime_type)
        {
            computer_item_ = computer_item;
            setData(mime_type, QByteArray());
        }

        LocalComputerItem* computerItem() const
        {
            return computer_item_;
        }

    private:
        LocalComputerItem* computer_item_ = nullptr;
    };

    class ComputerDrag final : public QDrag
    {
    public:
        explicit ComputerDrag(QObject* drag_source = nullptr)
            : QDrag(drag_source)
        {
            // Nothing
        }

        void setComputerItem(LocalComputerItem* computer_item, const QString& mime_type)
        {
            ComputerMimeData* mime_data = new ComputerMimeData();
            mime_data->setComputerItem(computer_item, mime_type);
            setMimeData(mime_data);
        }
    };

signals:
    void sig_computerDoubleClicked(qint64 computer_id);
    void sig_currentComputerChanged(qint64 computer_id);
    void sig_computerContextMenu(qint64 computer_id, const QPoint& pos);

private slots:
    void onHeaderContextMenu(const QPoint& pos);

private:
    Ui::LocalGroupWidget ui;

    Q_DISABLE_COPY_MOVE(LocalGroupWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_LOCAL_GROUP_WIDGET_H
