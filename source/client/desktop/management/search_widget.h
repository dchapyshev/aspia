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

#ifndef CLIENT_DESKTOP_MANAGEMENT_SEARCH_WIDGET_H
#define CLIENT_DESKTOP_MANAGEMENT_SEARCH_WIDGET_H

#include <QHash>
#include <QList>
#include <QPoint>
#include <QTreeWidgetItem>

#include "client/config.h"
#include "client/router.h"
#include "client/search_page_model.h"
#include "client/desktop/management/content_widget.h"

class IconTextButton;
class QComboBox;
class QLabel;
class QStatusBar;
class QTimer;
class QTreeWidget;

class SearchWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() final;

    class Item : public QTreeWidgetItem
    {
    public:
        enum class Type { LOCAL, ROUTER };

        Item(Type type, QTreeWidget* parent)
            : QTreeWidgetItem(parent),
              type_(type)
        {
            // Nothing
        }

        Type type() const { return type_; }

        // Local entry id, or -1 for router hosts (host_.id() defaults to -1 and is never set).
        qint64 entryId() const { return host_.id(); }
        HostConfig hostConfig() const { return host_; }

    protected:
        HostConfig host_;

    private:
        const Type type_;
    };

    class LocalItem final : public Item
    {
    public:
        LocalItem(const HostConfig& host, const QString& group_path, QTreeWidget* parent);

        qint64 groupId() const { return host_.groupId(); }
        QString computerName() const { return host_.name(); }
        void updateFrom(const HostConfig& host, const QString& group_path);
    };

    class RouterItem final : public Item
    {
    public:
        RouterItem(qint64 router_id, const Router::Host& host, const QString& source_label,
                   QTreeWidget* parent);
    };

    void search(const QString& query);
    void clear();
    Item* currentItem();
    QString currentQuery() const { return current_query_; }
    void setCurrentHost(qint64 entry_id);
    void refreshItem(qint64 entry_id);
    void removeItem(qint64 entry_id);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

signals:
    void sig_activated();
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& pos);

private slots:
    void onHeaderContextMenu(const QPoint& pos);
    void dispatchRouterSearch();
    void onPageChanged(int index);
    void onPrevClicked();
    void onNextClicked();

private:
    class HighlightDelegate;

    LocalItem* findItemByEntryId(qint64 entry_id) const;
    void updateStatusLabels();

    // Counts the matches of the local address book and of every online router, in that order, and
    // hands the counts to the page model. The page itself is fetched once every source answered.
    void countSources();
    void onSourceCounted(int slot, qint64 match_count);

    // Asks every source for the window of the current page and shows the windows once they are
    // all in, so the rows never appear in the wrong order.
    void fetchCurrentPage();
    void showCurrentPage();
    void updatePagination();

    QTreeWidget* tree_host_ = nullptr;
    QLabel* status_results_label_ = nullptr;
    HighlightDelegate* highlight_delegate_ = nullptr;
    QTimer* router_search_timer_ = nullptr;
    QComboBox* combo_page_ = nullptr;
    IconTextButton* button_prev_ = nullptr;
    IconTextButton* button_next_ = nullptr;
    QString current_query_;

    // One source of matches. The local address book comes first and each online router follows,
    // ordered by its id, so the whole result has an order that does not depend on which router
    // answers first.
    struct Source
    {
        qint64 router_id = 0; // 0 - the local address book.
        QString label;
        qint64 match_count = -1; // -1 until the source answered; it drops out if it never does.
    };

    // The window of the current page that one source contributes, and the rows it answered with.
    struct PageSlice
    {
        int source = -1;
        qint64 offset = 0;
        qint64 count = 0;
        bool ready = false;
        QList<Router::Host> router_hosts;
    };

    SearchPageModel page_model_;
    QList<Source> sources_;
    QList<PageSlice> page_slices_;
    QList<HostConfig> local_matches_;
    QHash<qint64, GroupConfig> local_groups_;

    // Replies of a query the user has already moved on from are dropped by this, and so are the
    // replies of an earlier page of the same query.
    quint64 generation_ = 0;

    Q_DISABLE_COPY_MOVE(SearchWidget)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_SEARCH_WIDGET_H
