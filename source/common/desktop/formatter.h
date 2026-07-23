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

#ifndef COMMON_DESKTOP_FORMATTER_H
#define COMMON_DESKTOP_FORMATTER_H

#include <QCoreApplication>
#include <QString>

#include <ctime>

#include "base/time_types.h"

class Formatter
{
    Q_DECLARE_TR_FUNCTIONS(Formatter)

public:
    // Formats a byte count as a localized "<value> <unit>" string using binary units (B/kB/MB/GB/TB).
    static QString sizeToString(qint64 size);

    // Formats a duration as a localized "<n days> <n hours> <n minutes> <n seconds>" string,
    // omitting the leading zero components.
    static QString delayToString(Seconds delay);

    // Formats a duration as "HH:mm:ss"; hours keep growing past 24 instead of wrapping.
    static QString durationToString(Seconds duration);

    // Formats a transfer rate measured in bytes per second using binary units (B/s, kB/s, ...).
    static QString transferSpeedToString(qint64 speed);

    // Formats a link rate measured in bits per second using decimal units (bps, Kbps, Mbps, Gbps).
    static QString networkSpeedToString(quint64 speed);

    // Formats a Unix timestamp in the user's locale short format.
    static QString timeToString(time_t time);

private:
    Q_DISABLE_COPY_MOVE(Formatter)
};

#endif // COMMON_DESKTOP_FORMATTER_H
