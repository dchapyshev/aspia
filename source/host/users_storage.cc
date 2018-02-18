//
// PROJECT:         Aspia
// FILE:            host/users_storage.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/users_storage.h"
#include "host/users_storage_registry.h"

namespace aspia {

// static
std::unique_ptr<UsersStorage> UsersStorage::Open()
{
    return std::make_unique<UsersStorageRegistry>();
}

} // namespace aspia
