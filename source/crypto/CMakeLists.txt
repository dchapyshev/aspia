#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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
    ${PROJECT_SOURCE_DIR}/crypto/big_num.cc
    ${PROJECT_SOURCE_DIR}/crypto/big_num.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_aes256_gcm.cc
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_aes256_gcm.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_chacha20_poly1305.cc
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_chacha20_poly1305.h
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor.h
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor_chacha20_poly1305.cc
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor_chacha20_poly1305.h
    ${PROJECT_SOURCE_DIR}/crypto/generic_hash.cc
    ${PROJECT_SOURCE_DIR}/crypto/generic_hash.h
    ${PROJECT_SOURCE_DIR}/crypto/openssl_util.cc
    ${PROJECT_SOURCE_DIR}/crypto/openssl_util.h
    ${PROJECT_SOURCE_DIR}/crypto/password_hash.cc
    ${PROJECT_SOURCE_DIR}/crypto/password_hash.h
    ${PROJECT_SOURCE_DIR}/crypto/random.cc
    ${PROJECT_SOURCE_DIR}/crypto/random.h
    ${PROJECT_SOURCE_DIR}/crypto/scoped_crypto_initializer.cc
    ${PROJECT_SOURCE_DIR}/crypto/scoped_crypto_initializer.h
    ${PROJECT_SOURCE_DIR}/crypto/secure_memory.cc
    ${PROJECT_SOURCE_DIR}/crypto/secure_memory.h
    ${PROJECT_SOURCE_DIR}/crypto/srp_constants.cc
    ${PROJECT_SOURCE_DIR}/crypto/srp_constants.h
    ${PROJECT_SOURCE_DIR}/crypto/srp_math.cc
    ${PROJECT_SOURCE_DIR}/crypto/srp_math.h)

list(APPEND SOURCE_CRYPTO_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/crypto/srp_math_unittest.cc)

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
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_crypto_tests COMMAND aspia_crypto_tests)
endif()

