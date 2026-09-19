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

#ifndef BASE_FILES_FILE_UTIL_H
#define BASE_FILES_FILE_UTIL_H

#include <QByteArray>
#include <QString>

#include <string_view>

bool writeFile(const QString& filename, const void* data, size_t size);
bool writeFile(const QString& filename, const QByteArray& buffer);
bool writeFile(const QString& filename, std::string_view buffer);

bool readFile(const QString& filename, QByteArray* buffer);
bool readFile(const QString& filename, std::string* buffer);

// Asks the system to remove |filename| at its next start, for a file that something else is still
// holding. A directory is removed only when it is empty, so its files are registered first. False
// is returned where the system cannot do this at all.
bool removeAtNextStart(const QString& filename);

#endif // BASE_FILES_FILE_UTIL_H
