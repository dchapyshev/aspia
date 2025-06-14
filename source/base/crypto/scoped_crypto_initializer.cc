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

#include "base/crypto/scoped_crypto_initializer.h"

#include "base/logging.h"

#include <openssl/crypto.h>
#include <openssl/ssl.h>

namespace base {

//--------------------------------------------------------------------------------------------------
ScopedCryptoInitializer::ScopedCryptoInitializer()
{
    int ret = OPENSSL_add_all_algorithms_noconf();
    if (ret != 1)
    {
        LOG(ERROR) << "OPENSSL_init_crypto failed";
        return;
    }

    initialized_ = true;
}

//--------------------------------------------------------------------------------------------------
ScopedCryptoInitializer::~ScopedCryptoInitializer()
{
    OPENSSL_cleanup();
}

} // namespace base
