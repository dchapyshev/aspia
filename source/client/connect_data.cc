//
// PROJECT:         Aspia
// FILE:            client/connect_data.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/connect_data.h"

#include "crypto/secure_memory.h"

namespace aspia {

ConnectData::~ConnectData()
{
    secureMemZero(&computer_name_);
    secureMemZero(&address_);
    secureMemZero(&user_name_);
    secureMemZero(&password_);
}

} // namespace aspia
