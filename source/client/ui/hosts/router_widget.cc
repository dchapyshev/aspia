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

#include <QIODevice>

#include "base/logging.h"

namespace client {

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

} // namespace client
