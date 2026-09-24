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

#ifndef CLIENT_CREDENTIAL_FILE_H
#define CLIENT_CREDENTIAL_FILE_H

#include <QList>
#include <QString>

class CredentialConfig;
class SecureString;

// Credentials chosen by the user in a file of their own, sealed with a password given on export.
// An import only reads the file, what goes into the address book is up to the caller.
class CredentialFile
{
public:
    enum class Result
    {
        SUCCESS,
        FILE_ERROR,
        INVALID_FORMAT,
        UNSUPPORTED_VERSION,
        WRONG_PASSWORD,
        UNREADABLE_RECORD,
        INTERNAL_ERROR,
    };

    static Result exportToFile(const QList<CredentialConfig>& credentials, const QString& file_path,
                               const SecureString& password);

    // |credentials| receives the records of the file without ids, and is left alone unless the
    // file was read.
    static Result importFromFile(const QString& file_path, const SecureString& password,
                                 QList<CredentialConfig>* credentials);

private:
    Q_DISABLE_COPY_MOVE(CredentialFile)
};

#endif // CLIENT_CREDENTIAL_FILE_H
