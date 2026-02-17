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

#ifndef BASE_SMBIOS_PARSER_H
#define BASE_SMBIOS_PARSER_H

#include <QByteArray>
#include <QString>

#include "base/smbios.h"

namespace base {

class SmbiosTableEnumerator
{
public:
    explicit SmbiosTableEnumerator(const QByteArray& smbios_data);
    ~SmbiosTableEnumerator();

    const SmbiosTable* table() const;

    bool isAtEnd() const;
    void advance();

    quint8 majorVersion() const;
    quint8 minorVersion() const;
    quint32 length() const;

private:
    SmbiosDump smbios_;

    quint8* start_ = nullptr;
    quint8* end_ = nullptr;
    quint8* pos_ = nullptr;

    Q_DISABLE_COPY(SmbiosTableEnumerator)
};

QString smbiosString(const SmbiosTable* table, quint8 number);

class SmbiosBios
{
public:
    explicit SmbiosBios(const SmbiosTable* table);

    QString vendor() const;
    QString version() const;
    QString releaseDate() const;

private:
    const SmbiosBiosTable* table_;
    Q_DISABLE_COPY(SmbiosBios)
};

class SmbiosBaseboard
{
public:
    explicit SmbiosBaseboard(const SmbiosTable* table);

    bool isValid() const;
    QString manufacturer() const;
    QString product() const;

private:
    const SmbiosBaseboardTable* table_;
    Q_DISABLE_COPY(SmbiosBaseboard)
};

class SmbiosMemoryDevice
{
public:
    explicit SmbiosMemoryDevice(const SmbiosTable* table);

    bool isValid() const;
    bool isPresent() const;
    QString location() const;
    QString manufacturer() const;
    quint64 size() const;
    QString type() const;
    QString formFactor() const;
    QString partNumber() const;
    quint32 speed() const;

private:
    const SmbiosMemoryDeviceTable* table_;
    Q_DISABLE_COPY(SmbiosMemoryDevice)
};

} // namespace base

#endif // BASE_SMBIOS_PARSER_H
