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

#include "console/mru.h"

namespace console {

namespace {

// Maximum number of entries in the list.
const int kMaxMruSize = 10;

//--------------------------------------------------------------------------------------------------
QString normalizedFilePath(const QString& file)
{
    QString normalized(file);
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return normalized;
}

//--------------------------------------------------------------------------------------------------
QStringList normalizedFileList(const QStringList& source_list)
{
    QStringList result;

    for (const auto& file : source_list)
        result.emplace_back(normalizedFilePath(file));

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void Mru::setRecentOpen(const QStringList& list)
{
    recent_list_ = normalizedFileList(list);

    // If the number of entries is more than the maximum, then delete the excess.
    while (recent_list_.size() > kMaxMruSize)
        recent_list_.pop_back();
}

//--------------------------------------------------------------------------------------------------
void Mru::setPinnedFiles(const QStringList& list)
{
    pinned_list_ = normalizedFileList(list);
}

//--------------------------------------------------------------------------------------------------
bool Mru::addRecentFile(const QString& file_path)
{
    QString normalized_path = normalizedFilePath(file_path);
    if (normalized_path.isEmpty())
        return false;

    // We are looking for a file in the list. If the file already exists in it, then delete it.
    for (auto it = recent_list_.begin(), it_end = recent_list_.end(); it != it_end; ++it)
    {
#if defined(Q_OS_WINDOWS)
        if (normalized_path.compare(*it, Qt::CaseInsensitive) == 0)
#else
        if (normalized_path.compare(*it, Qt::CaseSensitive) == 0)
#endif
        {
            recent_list_.erase(it);
            break;
        }
    }

    // Add the file to the top of the list.
    recent_list_.push_front(normalized_path);

    // We keep a limited number of entries on the list. If the number of records exceeds the
    // maximum, then delete the item at the end of the list.
    if (recent_list_.size() > kMaxMruSize)
        recent_list_.pop_back();

    return true;
}

//--------------------------------------------------------------------------------------------------
void Mru::pinFile(const QString& file_path)
{
    QString normalized_path = normalizedFilePath(file_path);

    if (isPinnedFile(normalized_path))
        return;

    pinned_list_.emplace_back(normalized_path);
}

//--------------------------------------------------------------------------------------------------
void Mru::unpinFile(const QString& file_path)
{
    QString normalized_path = normalizedFilePath(file_path);

    if (!isPinnedFile(normalized_path))
        return;

    pinned_list_.removeAll(normalized_path);
}

//--------------------------------------------------------------------------------------------------
bool Mru::isPinnedFile(const QString& file_path) const
{
    QString normalized_path = normalizedFilePath(file_path);

#if defined(Q_OS_WINDOWS)
    return pinned_list_.contains(normalized_path, Qt::CaseInsensitive);
#else
    return pinned_list_.contains(normalized_path, Qt::CaseSensitive);
#endif
}

//--------------------------------------------------------------------------------------------------
void Mru::clearRecentOpen()
{
    recent_list_.clear();
}

} // namespace console
