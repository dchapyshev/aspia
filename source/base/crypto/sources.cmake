#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

collect_sources(SOURCE_BASE_CRYPTO
    base32.cc
    base32.h
    big_num.cc
    big_num.h
    data_cryptor.cc
    data_cryptor.h
    datagram_decryptor.cc
    datagram_decryptor.h
    datagram_encryptor.cc
    datagram_encryptor.h
    generic_hash.cc
    generic_hash.h
    key_pair.cc
    key_pair.h
    large_number_increment.cc
    large_number_increment.h
    stream_decryptor.cc
    stream_decryptor.h
    stream_encryptor.cc
    stream_encryptor.h
    openssl_util.cc
    openssl_util.h
    os_crypt.h
    password_generator.cc
    password_generator.h
    password_hash.cc
    password_hash.h
    private_key_cryptor.cc
    private_key_cryptor.h
    random.cc
    random.h
    sealed_box.cc
    sealed_box.h
    secure_byte_array.cc
    secure_byte_array.h
    secure_memory.cc
    secure_memory.h
    secure_string.cc
    secure_string.h
    srp_math.cc
    srp_math.h
    totp.cc
    totp.h)

if (WIN32)
    collect_sources(SOURCE_BASE_CRYPTO os_crypt_win.cc)
endif()

if (APPLE)
    collect_sources(SOURCE_BASE_CRYPTO os_crypt_mac.mm)
endif()

if (LINUX OR ANDROID)
    collect_sources(SOURCE_BASE_CRYPTO os_crypt_linux.cc)
endif()

collect_sources(SOURCE_BASE_CRYPTO_TESTS
    base32_unittest.cc
    big_num_unittest.cc
    stream_cryptor_unittest.cc
    data_cryptor_unittest.cc
    datagram_cryptor_unittest.cc
    generic_hash_unittest.cc
    key_pair_unittest.cc
    large_number_increment_unittest.cc
    password_generator_unittest.cc
    password_hash_unittest.cc
    private_key_cryptor_unittest.cc
    sealed_box_unittest.cc
    srp_math_unittest.cc
    totp_unittest.cc)
