//
// SmartCafe Project
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

#ifndef CONSOLE_MRU_H
#define CONSOLE_MRU_H

#include <QStringList>

namespace console {

class Mru
{
public:
    Mru() = default;
    ~Mru() = default;

    const QStringList& recentOpen() const { return recent_list_; }
    void setRecentOpen(const QStringList& list);

    const QStringList& pinnedFiles() const { return pinned_list_; }
    void setPinnedFiles(const QStringList& list);

    bool addRecentFile(const QString& file_path);

    void pinFile(const QString& file_path);
    void unpinFile(const QString& file_path);
    bool isPinnedFile(const QString& file_path) const;

    void clearRecentOpen();

private:
    QStringList recent_list_;
    QStringList pinned_list_;
};

} // namespace console

#endif // CONSOLE_MRU_H
