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

#ifndef CLIENT_UI_HOSTS_SEARCH_WIDGET_H
#define CLIENT_UI_HOSTS_SEARCH_WIDGET_H

#include <QPoint>
#include <QTreeWidgetItem>

#include "client/config.h"
#include "client/ui/hosts/content_widget.h"

class QLabel;
class QStatusBar;
class QTreeWidget;

class SearchWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() final;

    class Item final : public QTreeWidgetItem
    {
    public:
        Item(const HostConfig& host, const QString& group_path, QTreeWidget* parent);

        qint64 computerId() const { return computer_.id(); }
        qint64 groupId() const { return computer_.groupId(); }
        QString computerName() const { return computer_.name(); }

        void updateFrom(const HostConfig& host, const QString& group_path);

    private:
        HostConfig computer_;
    };

    void search(const QString& query);
    void clear();
    Item* currentItem();
    QString currentQuery() const { return current_query_; }
    void setCurrentComputer(qint64 entry_id);
    void refreshItem(qint64 entry_id);
    void removeItem(qint64 entry_id);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

signals:
    void sig_doubleClicked(qint64 entry_id);
    void sig_currentChanged(qint64 entry_id);
    void sig_contextMenu(qint64 entry_id, const QPoint& pos);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onHeaderContextMenu(const QPoint& pos);

private:
    class HighlightDelegate;

    Item* findItemByComputerId(qint64 entry_id) const;
    void updateStatusLabels();

    QTreeWidget* tree_computer_ = nullptr;
    QLabel* status_results_label_ = nullptr;
    HighlightDelegate* highlight_delegate_ = nullptr;
    QString current_query_;

    Q_DISABLE_COPY_MOVE(SearchWidget)
};

#endif // CLIENT_UI_HOSTS_SEARCH_WIDGET_H
