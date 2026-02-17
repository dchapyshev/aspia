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

#ifndef BASE_WIN_PRINTER_ENUMERATOR_H
#define BASE_WIN_PRINTER_ENUMERATOR_H

#include <QByteArray>
#include <QString>

struct _PRINTER_INFO_2W;

namespace base {

class PrinterEnumerator
{
public:
    PrinterEnumerator();
    ~PrinterEnumerator();

    bool isAtEnd() const;
    void advance();

    bool isDefault() const;
    bool isShared() const;
    QString name() const;
    QString shareName() const;
    QString portName() const;
    QString driverName() const;
    QString deviceName() const;
    int paperWidth() const;
    int paperHeight() const;
    int printQuality() const;
    QString printProcessor() const;
    QString dataType() const;
    QString serverName() const;
    QString location() const;
    QString comment() const;
    int jobsCount() const;

private:
    QByteArray info_buffer_;
    std::wstring default_printer_;

    _PRINTER_INFO_2W* info_ = nullptr;
    int count_ = 0;
    int current_ = 0;

    Q_DISABLE_COPY(PrinterEnumerator)
};

} // namespace base

#endif // BASE_WIN_PRINTER_ENUMERATOR_H
