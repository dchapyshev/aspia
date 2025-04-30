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

#ifndef BASE_SMBIOS_PARSER_H
#define BASE_SMBIOS_PARSER_H

#include "base/macros_magic.h"
#include "base/smbios.h"

#include <QByteArray>
#include <QString>

namespace base {

class SmbiosTableEnumerator
{
public:
    explicit SmbiosTableEnumerator(const QByteArray& smbios_data);
    ~SmbiosTableEnumerator();

    const SmbiosTable* table() const;

    bool isAtEnd() const;
    void advance();

    uint8_t majorVersion() const;
    uint8_t minorVersion() const;
    uint32_t length() const;

private:
    SmbiosDump smbios_;

    uint8_t* start_ = nullptr;
    uint8_t* end_ = nullptr;
    uint8_t* pos_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SmbiosTableEnumerator);
};

QString smbiosString(const SmbiosTable* table, uint8_t number);

class SmbiosBios
{
public:
    explicit SmbiosBios(const SmbiosTable* table);

    QString vendor() const;
    QString version() const;
    QString releaseDate() const;

private:
    const SmbiosBiosTable* table_;
    DISALLOW_COPY_AND_ASSIGN(SmbiosBios);
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
    DISALLOW_COPY_AND_ASSIGN(SmbiosBaseboard);
};

class SmbiosMemoryDevice
{
public:
    explicit SmbiosMemoryDevice(const SmbiosTable* table);

    bool isValid() const;
    bool isPresent() const;
    QString location() const;
    QString manufacturer() const;
    uint64_t size() const;
    QString type() const;
    QString formFactor() const;
    QString partNumber() const;
    uint32_t speed() const;

private:
    const SmbiosMemoryDeviceTable* table_;
    DISALLOW_COPY_AND_ASSIGN(SmbiosMemoryDevice);
};

} // namespace base

#endif // BASE_SMBIOS_PARSER_H
