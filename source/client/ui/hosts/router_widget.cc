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

#include "client/ui/hosts/router_widget.h"

#include <QDateTime>
#include <QIODevice>

#include "base/logging.h"
#include "proto/router_admin.h"

namespace client {

namespace {

class RelayTreeItem final : public QTreeWidgetItem
{
public:
    explicit RelayTreeItem(const proto::router::RelayInfo& info)
    {
        updateItem(info);

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setIcon(0, QIcon(":/img/stack.svg"));
        setText(0, QString::fromStdString(info.ip_address()));
        setText(1, time);

        const proto::peer::Version& version = info.version();

        setText(3, QString("%1.%2.%3.%4")
            .arg(version.major()).arg(version.minor()).arg(version.patch()).arg(version.revision()));
        setText(4, QString::fromStdString(info.computer_name()));
        setText(5, QString::fromStdString(info.architecture()));
        setText(6, QString::fromStdString(info.os_name()));
    }

    void updateItem(const proto::router::RelayInfo& updated_info)
    {
        info = updated_info;
        setText(2, QString::number(info.pool_size()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        if (treeWidget()->sortColumn() == 1)
        {
            const RelayTreeItem* other_item = static_cast<const RelayTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::RelayInfo info;

private:
    Q_DISABLE_COPY_MOVE(RelayTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWidget::RouterWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER, parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::itemCount() const
{
    return 0;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        stream << ui.tree_relays->header()->saveState();
        stream << ui.splitter->saveState();
        stream << ui.tree_peers->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray relays_columns_state;
    QByteArray splitter_state;
    QByteArray peers_columns_state;

    stream >> relays_columns_state;
    stream >> splitter_state;
    stream >> peers_columns_state;

    if (!relays_columns_state.isEmpty())
        ui.tree_relays->header()->restoreState(relays_columns_state);

    if (!splitter_state.isEmpty())
    {
        ui.splitter->restoreState(splitter_state);
    }
    else
    {
        int side_size = height() / 2;

        QList<int> sizes;
        sizes.emplace_back(side_size);
        sizes.emplace_back(side_size);
        ui.splitter->setSizes(sizes);
    }

    if (!peers_columns_state.isEmpty())
        ui.tree_peers->header()->restoreState(peers_columns_state);
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterWidget::delayToString(quint64 delay)
{
    quint64 days = (delay / 86400);
    quint64 hours = (delay % 86400) / 3600;
    quint64 minutes = ((delay % 86400) % 3600) / 60;
    quint64 seconds = ((delay % 86400) % 3600) % 60;

    QString seconds_string = tr("%n seconds", "", static_cast<int>(seconds));
    QString minutes_string = tr("%n minutes", "", static_cast<int>(minutes));
    QString hours_string = tr("%n hours", "", static_cast<int>(hours));

    if (days)
    {
        QString days_string = tr("%n days", "", static_cast<int>(days));
        return days_string + ' ' + hours_string + ' ' + minutes_string + ' ' + seconds_string;
    }

    if (hours)
        return hours_string + ' ' + minutes_string + ' ' + seconds_string;

    if (minutes)
        return minutes_string + ' ' + seconds_string;

    return seconds_string;
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterWidget::sizeToString(qint64 size)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (size >= kTB)
        units = tr("TB"), divider = kTB;
    else if (size >= kGB)
        units = tr("GB"), divider = kGB;
    else if (size >= kMB)
        units = tr("MB"), divider = kMB;
    else if (size >= kKB)
        units = tr("kB"), divider = kKB;
    else
        units = tr("B"), divider = 1;

    return QString("%1 %2").arg(double(size) / double(divider), 0, 'g', 4).arg(units);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayListReceived(const proto::router::RelayList& relays)
{
    auto has_with_id = [](const proto::router::RelayList& relays, qint64 entry_id)
    {
        for (int i = 0; i < relays.relay_size(); ++i)
        {
            if (relays.relay(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all relays that are not in the list.
    for (int i = ui.tree_relays->topLevelItemCount() - 1; i >= 0; --i)
    {
        RelayTreeItem* item = static_cast<RelayTreeItem*>(ui.tree_relays->topLevelItem(i));

        if (!has_with_id(relays, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < relays.relay_size(); ++i)
    {
        const proto::router::RelayInfo& info = relays.relay(i);
        bool found = false;

        for (int j = 0; j < ui.tree_relays->topLevelItemCount(); ++j)
        {
                RelayTreeItem* item = static_cast<RelayTreeItem*>(ui.tree_relays->topLevelItem(j));
                if (item->info.entry_id() == info.entry_id())
                {
                    item->updateItem(info);
                    found = true;
                    break;
                }
        }

        if (!found)
            ui.tree_relays->addTopLevelItem(new RelayTreeItem(info));
    }
}

} // namespace client
