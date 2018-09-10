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
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.cc
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.h
    ${PROJECT_SOURCE_DIR}/base/bitset.h
    ${PROJECT_SOURCE_DIR}/base/const_buffer.h
    ${PROJECT_SOURCE_DIR}/base/cpuid.cc
    ${PROJECT_SOURCE_DIR}/base/cpuid.h
    ${PROJECT_SOURCE_DIR}/base/guid.cc
    ${PROJECT_SOURCE_DIR}/base/guid.h
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.cc
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.h
    ${PROJECT_SOURCE_DIR}/base/locale_loader.cc
    ${PROJECT_SOURCE_DIR}/base/locale_loader.h
    ${PROJECT_SOURCE_DIR}/base/logging.cc
    ${PROJECT_SOURCE_DIR}/base/logging.h
    ${PROJECT_SOURCE_DIR}/base/macros_magic.h
    ${PROJECT_SOURCE_DIR}/base/message_serialization.h
    ${PROJECT_SOURCE_DIR}/base/service.h
    ${PROJECT_SOURCE_DIR}/base/service_controller.cc
    ${PROJECT_SOURCE_DIR}/base/service_controller.h
    ${PROJECT_SOURCE_DIR}/base/service_impl.h
    ${PROJECT_SOURCE_DIR}/base/service_impl_win.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf.h
    ${PROJECT_SOURCE_DIR}/base/string_util.cc
    ${PROJECT_SOURCE_DIR}/base/string_util.h
    ${PROJECT_SOURCE_DIR}/base/string_util_constants.cc
    ${PROJECT_SOURCE_DIR}/base/string_util_constants.h
    ${PROJECT_SOURCE_DIR}/base/typed_buffer.h
    ${PROJECT_SOURCE_DIR}/base/unicode.cc
    ${PROJECT_SOURCE_DIR}/base/unicode.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/base/aligned_memory_unittest.cc
    ${PROJECT_SOURCE_DIR}/base/guid_unittest.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf_unittest.cc)

list(APPEND SOURCE_BASE_WIN
    ${PROJECT_SOURCE_DIR}/base/win/registry.cc
    ${PROJECT_SOURCE_DIR}/base/win/registry.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_com_initializer.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_gdi_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_hdc.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_local.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_user_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_wts_memory.h
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.cc
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.h)

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

