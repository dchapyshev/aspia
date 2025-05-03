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

#include "client/ui/sys_info/sys_info_widget.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QLocale>
#include <QTreeWidgetItem>
#include <QUrl>

namespace client {

namespace {

//--------------------------------------------------------------------------------------------------
void copyTextToClipboard(const QString& text)
{
    if (text.isEmpty())
        return;

    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
QString encodeUrl(const QString& str)
{
    if (str.isEmpty())
        return QString();

    QString result;

    for (const auto ch : str)
    {
        if (ch.isDigit() || ch.isLetter() || ch == QLatin1Char('-') || ch == QLatin1Char('_') ||
            ch == QLatin1Char('.') || ch == QLatin1Char('~'))
        {
            result += ch;
        }
        else if (ch == QLatin1Char(' '))
        {
            result += QLatin1Char('+');
        }
        else
        {
            result += QLatin1Char('%') + QString::number(ch.unicode(), 16);
        }
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidget::SysInfoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
proto::system_info::SystemInfoRequest SysInfoWidget::request() const
{
    proto::system_info::SystemInfoRequest system_info_request;
    system_info_request.set_category(category());
    return system_info_request;
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidget::sizeToString(qint64 size)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (size >= kTB)
    {
        units = tr("TB");
        divider = kTB;
    }
    else if (size >= kGB)
    {
        units = tr("GB");
        divider = kGB;
    }
    else if (size >= kMB)
    {
        units = tr("MB");
        divider = kMB;
    }
    else if (size >= kKB)
    {
        units = tr("kB");
        divider = kKB;
    }
    else
    {
        units = tr("B");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
QString SysInfoWidget::delayToString(quint64 delay)
{
    quint64 days = (delay / 86400);
    quint64 hours = (delay % 86400) / 3600;
    quint64 minutes = ((delay % 86400) % 3600) / 60;
    quint64 seconds = ((delay % 86400) % 3600) % 60;

    QString seconds_string = tr("%n seconds", "", static_cast<int>(seconds));
    QString minutes_string = tr("%n minutes", "", static_cast<int>(minutes));
    QString hours_string = tr("%n hours", "", static_cast<int>(hours));

    if (!days)
    {
        if (!hours)
        {
            if (!minutes)
            {
                return seconds_string;
            }
            else
            {
                return minutes_string + QLatin1Char(' ') + seconds_string;
            }
        }
        else
        {
            return hours_string + QLatin1Char(' ') +
                   minutes_string + QLatin1Char(' ') +
                   seconds_string;
        }
    }
    else
    {
        QString days_string = tr("%n days", "", static_cast<int>(days));

        return days_string + QLatin1Char(' ') +
               hours_string + QLatin1Char(' ') +
               minutes_string + QLatin1Char(' ') +
               seconds_string;
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidget::speedToString(quint64 speed)
{
    static const quint64 kKbps = 1000ULL;
    static const quint64 kMbps = kKbps * 1000ULL;
    static const quint64 kGbps = kMbps * 1000ULL;

    QString units;
    quint64 divider;

    if (speed >= kGbps)
    {
        units = tr("Gbps");
        divider = kGbps;
    }
    else if (speed >= kMbps)
    {
        units = tr("Mbps");
        divider = kMbps;
    }
    else if (speed >= kKbps)
    {
        units = tr("Kbps");
        divider = kKbps;
    }
    else
    {
        units = tr("bps");
        divider = 1;
    }

    return QString("%1 %2")
        .arg(static_cast<double>(speed) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidget::timeToString(time_t time)
{
    return QLocale::system().toString(QDateTime::fromSecsSinceEpoch(time), QLocale::ShortFormat);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyRow(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QString result;

    int column_count = item->columnCount();
    if (column_count > 2)
    {
        for (int i = 0; i < column_count; ++i)
        {
            QString text = item->text(i);

            if (!text.isEmpty())
                result += text + QLatin1Char(' ');
        }

        result.chop(1);
    }
    else
    {
        result = item->text(0) + QLatin1String(": ") + item->text(1);
    }

    copyTextToClipboard(result);
}

//--------------------------------------------------------------------------------------------------
// static
void SysInfoWidget::searchInGoogle(const QString& request)
{
    QUrl find_url(QString("https://www.google.com/search?q=%1").arg(encodeUrl(request)));
    QDesktopServices::openUrl(find_url);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidget::copyColumn(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    copyTextToClipboard(item->text(column));
}

} // namespace client
