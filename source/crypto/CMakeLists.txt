#
# Aspia Project
# Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

list(APPEND SOURCE_CRYPTO
    big_num.cc
    big_num.h
    data_cryptor.h
    data_cryptor_chacha20_poly1305.cc
    data_cryptor_chacha20_poly1305.h
    data_cryptor_fake.cc
    data_cryptor_fake.h
    generic_hash.cc
    generic_hash.h
    large_number_increment.cc
    large_number_increment.h
    message_decryptor.h
    message_decryptor_fake.cc
    message_decryptor_fake.h
    message_decryptor_openssl.cc
    message_decryptor_openssl.h
    message_encryptor.h
    message_encryptor_fake.cc
    message_encryptor_fake.h
    message_encryptor_openssl.cc
    message_encryptor_openssl.h
    openssl_util.cc
    openssl_util.h
    password_generator.cc
    password_generator.h
    password_hash.cc
    password_hash.h
    random.cc
    random.h
    scoped_crypto_initializer.cc
    scoped_crypto_initializer.h
    secure_memory.cc
    secure_memory.h
    srp_constants.cc
    srp_constants.h
    srp_math.cc
    srp_math.h)

list(APPEND SOURCE_CRYPTO_UNIT_TESTS
    crypto_tests_main.cc
    cryptor_unittest.cc
    data_cryptor_unittest.cc
    generic_hash_unittest.cc
    large_number_increment_unittest.cc
    password_generator_unittest.cc
    password_hash_unittest.cc
    srp_math_unittest.cc)

source_group("" FILES ${SOURCE_CRYPTO} ${SOURCE_CRYPTO_UNIT_TESTS})

add_library(aspia_crypto STATIC ${SOURCE_CRYPTO})
target_link_libraries(aspia_crypto
    aspia_base
    ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_crypto_tests ${SOURCE_CRYPTO_UNIT_TESTS})
    target_link_libraries(aspia_crypto_tests
        aspia_base
        aspia_crypto
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${WINDOWS_LIBS}
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_crypto_tests COMMAND aspia_crypto_tests)
endif()

