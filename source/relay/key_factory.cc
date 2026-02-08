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

#include "relay/key_factory.h"

namespace relay {

//--------------------------------------------------------------------------------------------------
KeyFactory::KeyFactory(QObject* parent)
    : QObject(parent),
      pool_(new KeyPool(this))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
KeyFactory::~KeyFactory()
{
    pool_->dettach();
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<KeyPool> KeyFactory::sharedKeyPool()
{
    return pool_;
}

//--------------------------------------------------------------------------------------------------
quint32 KeyFactory::addKey(SessionKey&& session_key)
{
    return pool_->addKey(std::move(session_key));
}

//--------------------------------------------------------------------------------------------------
bool KeyFactory::removeKey(quint32 key_id)
{
    return pool_->removeKey(key_id);
}

//--------------------------------------------------------------------------------------------------
void KeyFactory::setKeyExpired(quint32 key_id)
{
    return pool_->setKeyExpired(key_id);
}

//--------------------------------------------------------------------------------------------------
std::optional<KeyFactory::Key> KeyFactory::key(
    quint32 key_id, const std::string& peer_public_key) const
{
    return pool_->key(key_id, peer_public_key);
}

//--------------------------------------------------------------------------------------------------
void KeyFactory::clear()
{
    pool_->clear();
}

} // namespace relay
