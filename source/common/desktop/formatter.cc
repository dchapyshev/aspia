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

#include "common/desktop/formatter.h"

#include <QDateTime>
#include <QLocale>

namespace {

constexpr qint64 kBinaryKB = 1024LL;
constexpr qint64 kBinaryMB = kBinaryKB * 1024LL;
constexpr qint64 kBinaryGB = kBinaryMB * 1024LL;
constexpr qint64 kBinaryTB = kBinaryGB * 1024LL;

constexpr quint64 kDecimalKbps = 1000ULL;
constexpr quint64 kDecimalMbps = kDecimalKbps * 1000ULL;
constexpr quint64 kDecimalGbps = kDecimalMbps * 1000ULL;

constexpr quint64 kSecondsInMinute = 60;
constexpr quint64 kSecondsInHour = 60 * kSecondsInMinute;
constexpr quint64 kSecondsInDay = 24 * kSecondsInHour;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QString Formatter::sizeToString(qint64 size)
{
    QString units;
    qint64 divider;

    if (size >= kBinaryTB)
        units = tr("TB"), divider = kBinaryTB;
    else if (size >= kBinaryGB)
        units = tr("GB"), divider = kBinaryGB;
    else if (size >= kBinaryMB)
        units = tr("MB"), divider = kBinaryMB;
    else if (size >= kBinaryKB)
        units = tr("kB"), divider = kBinaryKB;
    else
        units = tr("B"), divider = 1;

    return QString("%1 %2")
        .arg(static_cast<double>(size) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
// static
QString Formatter::delayToString(quint64 delay)
{
    quint64 days = delay / kSecondsInDay;
    quint64 hours = (delay % kSecondsInDay) / kSecondsInHour;
    quint64 minutes = (delay % kSecondsInHour) / kSecondsInMinute;
    quint64 seconds = delay % kSecondsInMinute;

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
QString Formatter::transferSpeedToString(qint64 speed)
{
    QString units;
    qint64 divider;

    if (speed >= kBinaryTB)
        units = tr("TB/s"), divider = kBinaryTB;
    else if (speed >= kBinaryGB)
        units = tr("GB/s"), divider = kBinaryGB;
    else if (speed >= kBinaryMB)
        units = tr("MB/s"), divider = kBinaryMB;
    else if (speed >= kBinaryKB)
        units = tr("kB/s"), divider = kBinaryKB;
    else
        units = tr("B/s"), divider = 1;

    return QString("%1 %2")
        .arg(static_cast<double>(speed) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
// static
QString Formatter::networkSpeedToString(quint64 speed)
{
    QString units;
    quint64 divider;

    if (speed >= kDecimalGbps)
        units = tr("Gbps"), divider = kDecimalGbps;
    else if (speed >= kDecimalMbps)
        units = tr("Mbps"), divider = kDecimalMbps;
    else if (speed >= kDecimalKbps)
        units = tr("Kbps"), divider = kDecimalKbps;
    else
        units = tr("bps"), divider = 1;

    return QString("%1 %2")
        .arg(static_cast<double>(speed) / static_cast<double>(divider), 0, 'g', 4)
        .arg(units);
}

//--------------------------------------------------------------------------------------------------
// static
QString Formatter::timeToString(time_t time)
{
    return QLocale::system().toString(QDateTime::fromSecsSinceEpoch(time), QLocale::ShortFormat);
}
