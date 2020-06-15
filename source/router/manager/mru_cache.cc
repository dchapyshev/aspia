//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/mru_cache.h"

namespace router {

MruCache::MruCache(int max_size)
    : max_size_(max_size)
{
    list_.reserve(max_size_);
}

MruCache::MruCache(const MruCache& other) = default;

MruCache& MruCache::operator=(const MruCache& other) = default;

MruCache::~MruCache() = default;

void MruCache::shrinkToSize(int new_size)
{
    while (list_.size() > new_size)
        list_.removeLast();
}

void MruCache::clear()
{
    list_.clear();
}

MruCache::Iterator MruCache::put(Entry&& entry)
{
    auto result = find(entry.address);
    if (result != list_.end())
    {
        list_.erase(result);
    }
    else
    {
        shrinkToSize(max_size_ - 1);
    }

    list_.push_front(std::move(entry));
    return list_.begin();
}

bool MruCache::hasIndex(int index) const
{
    return index >= 0 && index < list_.size();
}

const MruCache::Entry& MruCache::at(int index) const
{
    return list_.at(index);
}

MruCache::ConstIterator MruCache::cfind(const QString& address) const
{
    for (auto it = list_.cbegin(); it != list_.cend(); ++it)
    {
        if (it->address.compare(address, Qt::CaseInsensitive) == 0)
            return it;
    }

    return list_.cend();
}

MruCache::Iterator MruCache::find(const QString& address)
{
    for (auto it = list_.begin(); it != list_.end(); ++it)
    {
        if (it->address.compare(address, Qt::CaseInsensitive) == 0)
            return it;
    }

    return list_.end();
}

} // namespace router
