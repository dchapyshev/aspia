//
// Aspia Project
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

#ifndef ROUTER_KEY_FACTORY_H
#define ROUTER_KEY_FACTORY_H

#include <QObject>

#include "base/macros_magic.h"
#include "base/shared_pointer.h"
#include "proto/router_common.pb.h"
#include "router/key_pool.h"
#include "router/session.h"

namespace router {

class KeyFactory final : public QObject
{
    Q_OBJECT

public:
    explicit KeyFactory(QObject* parent = nullptr);
    ~KeyFactory();

    base::SharedPointer<KeyPool> sharedKeyPool();

    using Credentials = KeyPool::Credentials;

    void addKey(Session::SessionId session_id, const proto::router::RelayKey& key);
    std::optional<Credentials> takeCredentials();
    void removeKeysForRelay(Session::SessionId session_id);
    void clear();
    size_t countForRelay(Session::SessionId session_id) const;
    size_t count() const;
    bool isEmpty() const;

signals:
    void sig_keyUsed(Session::SessionId session_id, quint32 key_id);

private:
    base::SharedPointer<KeyPool> pool_;

    DISALLOW_COPY_AND_ASSIGN(KeyFactory);
};

} // namespace router

#endif // ROUTER_KEY_FACTORY_H
