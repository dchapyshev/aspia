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

#ifndef CLIENT_ADDRESS_BOOK_IMPORTER_H
#define CLIENT_ADDRESS_BOOK_IMPORTER_H

#include <QCoreApplication>
#include <QString>

class QWidget;

namespace client {

class AddressBookImporter
{
    Q_DECLARE_TR_FUNCTIONS(AddressBookImporter)

public:
    // Imports the address book from |file_path| into the root group.
    // Shows password dialog and error/success messages on |parent|.
    // Returns true if at least one item was imported.
    static bool import(QWidget* parent, const QString& file_path);

private:
    Q_DISABLE_COPY_MOVE(AddressBookImporter)
};

} // namespace client

#endif // CLIENT_ADDRESS_BOOK_IMPORTER_H
