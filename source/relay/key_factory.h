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

#ifndef RELAY_KEY_FACTORY_H
#define RELAY_KEY_FACTORY_H

#include <QObject>

#include "relay/key_pool.h"
#include "relay/session_key.h"

#include <optional>

namespace relay {

class KeyFactory final : public QObject
{
    Q_OBJECT

public:
    explicit KeyFactory(QObject* parent = nullptr);
    ~KeyFactory();

    using Key = KeyPool::Key;

    std::shared_ptr<KeyPool> sharedKeyPool();

    quint32 addKey(SessionKey&& session_key);
    bool removeKey(quint32 key_id);
    void setKeyExpired(quint32 key_id);
    std::optional<Key> key(quint32 key_id, const std::string& peer_public_key) const;
    void clear();

signals:
    void sig_keyExpired(quint32 key_id);

private:
    std::shared_ptr<KeyPool> pool_;

    DISALLOW_COPY_AND_ASSIGN(KeyFactory);
};

} // namespace relay

#endif // RELAY_KEY_FACTORY_H
