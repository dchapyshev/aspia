//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/data_cryptor_chacha20_poly1305.h"

#include <gtest/gtest.h>

namespace crypto {

TEST(DataCryptorChaCha20Poly1305Test, TestVector)
{
    const std::string_view key = "1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014";
    const std::string_view message = "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014"
                                     "5ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014";

    std::unique_ptr<DataCryptor> cryptor = std::make_unique<DataCryptorChaCha20Poly1305>(key);

    std::string encrypted_message;
    bool ret = cryptor->encrypt(message, &encrypted_message);
    ASSERT_TRUE(ret);
    ASSERT_EQ(encrypted_message.size(), message.size() + 28);

    std::string decrypted_message;

    ret = cryptor->decrypt(encrypted_message, &decrypted_message);
    ASSERT_TRUE(ret);
    ASSERT_EQ(decrypted_message.size(), message.size());
    ASSERT_EQ(decrypted_message, message);
}

TEST(DataCryptorChaCha20Poly1305Test, WrongKey)
{
    const std::string_view key = "1ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014";
    const std::string_view wrong_key = "2ce26794165a808ec425684e9384c27c22499512a513da8b455bd39746dc5014";
    const std::string_view message = "3da8b455bd39746dc50145ce26794165a808ec425684e9384c27c2249951256812125683";

    std::unique_ptr<DataCryptor> cryptor1 =
        std::make_unique<DataCryptorChaCha20Poly1305>(key);
    std::unique_ptr<DataCryptor> cryptor2 =
        std::make_unique<DataCryptorChaCha20Poly1305>(wrong_key);

    std::string encrypted_message;
    bool ret = cryptor1->encrypt(message, &encrypted_message);
    ASSERT_TRUE(ret);
    ASSERT_EQ(encrypted_message.size(), message.size() + 28);

    std::string decrypted_message;

    ret = cryptor2->decrypt(encrypted_message, &decrypted_message);
    ASSERT_FALSE(ret);
}

} // namespace crypto
