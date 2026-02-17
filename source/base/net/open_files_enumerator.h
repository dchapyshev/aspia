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

#ifndef BASE_NET_OPEN_FILES_ENUMERATOR_H
#define BASE_NET_OPEN_FILES_ENUMERATOR_H

#include <QString>

struct _FILE_INFO_3;

namespace base {

class OpenFilesEnumerator
{
public:
    OpenFilesEnumerator();
    ~OpenFilesEnumerator();

    bool isAtEnd() const;
    void advance();

    quint32 id() const;
    QString userName() const;
    quint32 lockCount() const;
    QString filePath() const;

private:
    _FILE_INFO_3* file_info_ = nullptr;
    quint32 total_entries_ = 0;
    quint32 current_pos_ = 0;

    Q_DISABLE_COPY(OpenFilesEnumerator)
};

} // namespace base

#endif // BASE_NET_OPEN_FILES_ENUMERATOR_H
