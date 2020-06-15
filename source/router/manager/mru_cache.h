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

#ifndef ROUTER__MANAGER__MRU_CACHE_H
#define ROUTER__MANAGER__MRU_CACHE_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace router {

class MruCache
{
public:
    struct Entry
    {
        QString address;
        uint16_t port;
        QString username;
        QString key_path;
    };

    using EntryList = QVector<Entry>;
    using Iterator = EntryList::iterator;
    using ConstIterator = EntryList::const_iterator;
    using ReverseIterator = EntryList::reverse_iterator;
    using ConstReverseIterator = EntryList::const_reverse_iterator;

    explicit MruCache(int max_size);
    MruCache(const MruCache& other);
    MruCache& operator=(const MruCache& other);
    ~MruCache();

    int maxSize() const { return max_size_; }
    int size() const { return list_.size(); }
    bool isEmpty() const { return list_.isEmpty(); }

    void shrinkToSize(int new_size);
    void clear();

    Iterator put(Entry&& entry);

    bool hasIndex(int index) const;
    const Entry& at(int index) const;

    ConstIterator cfind(const QString& address) const;
    Iterator find(const QString& address);

    ConstIterator cbegin() const { return list_.cbegin(); }
    Iterator begin() { return list_.begin(); }
    ConstReverseIterator crbegin() const { return list_.crbegin(); }
    ReverseIterator rbegin() { return list_.rbegin(); }

    ConstIterator cend() const { return list_.cend(); }
    Iterator end() { return list_.end(); }
    ConstReverseIterator crend() const { return list_.crend(); }
    ReverseIterator rend() { return list_.rend(); }

private:
    int max_size_;
    EntryList list_;
};

} // namespace router

#endif // ROUTER__MANAGER__MRU_CACHE_H
