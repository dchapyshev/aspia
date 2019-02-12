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
    bitset.h
    const_buffer.h
    cpuid.cc
    cpuid.h
    debug.cc
    debug.h
    logging.cc
    logging.h
    macros_magic.h
    power_controller.h
    power_controller_win.cc
    qt_logging.cc
    qt_logging.h
    scoped_clear_last_error.cc
    scoped_clear_last_error.h
    service.h
    service_controller.cc
    service_controller.h
    service_impl.h
    service_impl_win.cc
    smbios.h
    smbios_parser.cc
    smbios_parser.h
    smbios_reader.h
    smbios_reader_win.cc
    string_printf.cc
    string_printf.h
    string_util.cc
    string_util.h
    string_util_constants.cc
    string_util_constants.h
    sys_info.cc
    sys_info.h
    sys_info_win.cc
    thread_checker.cc
    thread_checker.h
    typed_buffer.h
    unicode.cc
    unicode.h
    xml_settings.cc
    xml_settings.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    aligned_memory_unittest.cc
    bitset_unittest.cc
    scoped_clear_last_error_unittest.cc
    string_printf_unittest.cc)

list(APPEND SOURCE_BASE_WIN
    win/desktop.cc
    win/desktop.h
    win/drive_enumerator.cc
    win/drive_enumerator.h
    win/printer_enumerator.cc
    win/printer_enumerator.h
    win/process_util.cc
    win/process_util.h
    win/registry.cc
    win/registry.h
    win/scoped_com_initializer.cc
    win/scoped_com_initializer.h
    win/scoped_gdi_object.h
    win/scoped_hdc.h
    win/scoped_impersonator.cc
    win/scoped_impersonator.h
    win/scoped_local.h
    win/scoped_object.h
    win/scoped_thread_desktop.cc
    win/scoped_thread_desktop.h
    win/scoped_user_object.h
    win/scoped_wts_memory.h
    win/security_helpers.cc
    win/security_helpers.h
    win/session_watcher.cc
    win/session_watcher.h)

list(APPEND SOURCE_BASE_WIN_UNIT_TESTS
    win/registry_unittest.cc
    win/scoped_object_unittest.cc)

source_group("" FILES ${SOURCE_BASE})
source_group("" FILES ${SOURCE_BASE_UNIT_TESTS})
source_group(win FILES ${SOURCE_BASE_WIN})
source_group(win FILES ${SOURCE_BASE_WIN_UNIT_TESTS})

add_library(aspia_base STATIC ${SOURCE_BASE} ${SOURCE_BASE_WIN})
target_link_libraries(aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_base_tests
        ${SOURCE_BASE_UNIT_TESTS}
        ${SOURCE_BASE_WIN_UNIT_TESTS})
    target_link_libraries(aspia_base_tests
        aspia_base
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
endif()

