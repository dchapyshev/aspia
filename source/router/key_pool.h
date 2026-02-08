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

#ifndef ROUTER_KEY_POOL_H
#define ROUTER_KEY_POOL_H

#include <QMap>
#include <QList>

#include <optional>

#include "proto/router.h"

namespace router {

class KeyFactory;

class KeyPool
{
public:
    ~KeyPool() = default;

    struct Credentials
    {
        qint64 session_id;
        proto::router::RelayKey key;
    };

    void addKey(qint64 session_id, const proto::router::RelayKey& key);
    std::optional<Credentials> takeCredentials();
    void removeKeysForRelay(qint64 session_id);
    void clear();
    size_t countForRelay(qint64 session_id) const;
    size_t count() const;
    bool isEmpty() const;

private:
    friend class KeyFactory;

    explicit KeyPool(KeyFactory* factory);
    void dettach();

    using Keys = QList<proto::router::RelayKey>;

    QMap<qint64, Keys> pool_;
    KeyFactory* factory_;

    Q_DISABLE_COPY(KeyPool)
};

} // namespace router

#endif // ROUTER_KEY_POOL_H
