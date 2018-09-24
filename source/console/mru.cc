//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"

namespace aspia {

namespace {

// Maximum number of entries in the list.
const int kMaxMruSize = 10;

} // namespace

void Mru::setFileList(const QStringList& list)
{
    list_ = list;

    // If the number of entries is more than the maximum, then delete the excess.
    while (list_.size() > kMaxMruSize)
        list_.pop_back();
}

void Mru::addItem(const QString& file_path)
{
    // We are looking for a file in the list. If the file already exists in it, then delete it.
    for (QStringList::iterator it = list_.begin(); it != list_.end(); ++it)
    {
#if defined(OS_WIN)
        if (file_path.compare(*it, Qt::CaseInsensitive) == 0)
#else
        if (file_path.compare(*it, Qt::CaseSensitive) == 0)
#endif
        {
            list_.erase(it);
            break;
        }
    }

    // Add the file to the top of the list.
    list_.push_front(file_path);

    // We keep a limited number of entries on the list. If the number of records exceeds the
    // maximum, then delete the item at the end of the list.
    if (list_.size() > kMaxMruSize)
        list_.pop_back();
}

} // namespace aspia
