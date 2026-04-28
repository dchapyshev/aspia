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
#include <QPoint>
#include <QPointer>

#include "client/online_checker/online_checker.h"
#include "client/ui/hosts/content_widget.h"
#include "ui_local_group_widget.h"

class QLabel;
class QStatusBar;

namespace client {

struct ComputerConfig;

class LocalGroupWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit LocalGroupWidget(QWidget* parent = nullptr);
    ~LocalGroupWidget() override;

    class Item : public QTreeWidgetItem
    {
    public:
        Item(const ComputerConfig& computer, QTreeWidget* parent);

        ComputerConfig& computer() { return computer_; }
        qint64 computerId() const { return computer_.id; }
        qint64 groupId() const { return computer_.group_id; }
        qint64 routerId() const { return computer_.router_id; }
        QString computerName() const { return computer_.name; }
        QString computerAddress() const { return computer_.address; }
        void setConnectTime(qint64 connect_time);
        void setOnlineStatus(bool online);
        void clearOnlineStatus();

        bool operator<(const QTreeWidgetItem& other) const final;

    private:
        ComputerConfig computer_;
    };

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

    Item* currentItem();
    void showGroup(qint64 group_id);
    void setConnectTime(qint64 computer_id, qint64 connect_time);

    // ContentWidget implementation.
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;
    bool canReload() const override { return true; }
    void reload() override;
    void attach(QStatusBar* statusbar) override;
    void detach(QStatusBar* statusbar) override;

    QString mimeType() const { return mime_type_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void sig_doubleClicked(qint64 computer_id);
    void sig_currentChanged(qint64 computer_id);
    void sig_contextMenu(qint64 computer_id, const QPoint& pos);

private slots:
    void onHeaderContextMenu(const QPoint& pos);
    void onOnlineCheckerResult(qint64 computer_id, bool online);
    void onOnlineCheckerFinished();

private:
    void startDrag();
    void updateStatusLabels();
    void startOnlineChecker();
    void stopOnlineChecker();
    void clearOnlineStatuses();

    Ui::LocalGroupWidget ui;
    QString mime_type_;
    QPoint start_pos_;

    qint64 current_group_id_ = -1;
    QLabel* status_groups_label_ = nullptr;
    QLabel* status_computers_label_ = nullptr;

    QPointer<OnlineChecker> online_checker_;

    Q_DISABLE_COPY_MOVE(LocalGroupWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_LOCAL_GROUP_WIDGET_H
