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

#include "router/key_factory.h"

namespace router {

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
base::SharedPointer<KeyPool> KeyFactory::sharedKeyPool()
{
    return pool_;
}

//--------------------------------------------------------------------------------------------------
void KeyFactory::addKey(Session::SessionId session_id, const proto::router::RelayKey& key)
{
    pool_->addKey(session_id, key);
}

//--------------------------------------------------------------------------------------------------
std::optional<KeyFactory::Credentials> KeyFactory::takeCredentials()
{
    return pool_->takeCredentials();
}

//--------------------------------------------------------------------------------------------------
void KeyFactory::removeKeysForRelay(Session::SessionId session_id)
{
    pool_->removeKeysForRelay(session_id);
}

//--------------------------------------------------------------------------------------------------
void KeyFactory::clear()
{
    pool_->clear();
}

//--------------------------------------------------------------------------------------------------
size_t KeyFactory::countForRelay(Session::SessionId session_id) const
{
    return pool_->countForRelay(session_id);
}

//--------------------------------------------------------------------------------------------------
size_t KeyFactory::count() const
{
    return pool_->count();
}

//--------------------------------------------------------------------------------------------------
bool KeyFactory::isEmpty() const
{
    return pool_->isEmpty();
}

} // namespace router
