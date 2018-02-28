//
// PROJECT:         Aspia
// FILE:            host/users_storage.cc
// LICENSE:         GNU Lesser General Public License 2.1
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
