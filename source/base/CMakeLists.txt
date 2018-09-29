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

list(APPEND SOURCE_BASE
    aligned_memory.cc
    aligned_memory.h
    base_paths.cc
    base_paths.h
    base64.cc
    base64.h
    base64_constants.cc
    base64_constants.h
    bitset.h
    const_buffer.h
    cpuid.cc
    cpuid.h
    guid.cc
    guid.h
    keycode_converter.cc
    keycode_converter.h
    logging.cc
    logging.h
    macros_magic.h
    service.h
    service_controller.cc
    service_controller.h
    service_impl.h
    service_impl_win.cc
    string_printf.cc
    string_printf.h
    string_util.cc
    string_util.h
    string_util_constants.cc
    string_util_constants.h
    typed_buffer.h
    unicode.cc
    unicode.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    aligned_memory_unittest.cc
    base64_unittest.cc
    guid_unittest.cc
    string_printf_unittest.cc)

list(APPEND SOURCE_BASE_WIN
    win/registry.cc
    win/registry.h
    win/scoped_com_initializer.h
    win/scoped_gdi_object.h
    win/scoped_hdc.h
    win/scoped_local.h
    win/scoped_object.h
    win/scoped_user_object.h
    win/scoped_wts_memory.h
    win/security_helpers.cc
    win/security_helpers.h)

source_group("" FILES ${SOURCE_BASE})
source_group("" FILES ${SOURCE_BASE_UNIT_TESTS})
source_group(win FILES ${SOURCE_BASE_WIN})

add_library(aspia_base STATIC ${SOURCE_BASE} ${SOURCE_BASE_WIN})
target_link_libraries(aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_base_tests ${SOURCE_BASE_UNIT_TESTS})
    target_link_libraries(aspia_base_tests
        aspia_base
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
endif()

