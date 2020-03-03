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

list(APPEND SOURCE_BASE
    base64.cc
    base64.h
    bitset.h
    command_line.cc
    command_line.h
    compiler_specific.h
    cpuid.cc
    cpuid.h
    debug.cc
    debug.h
    endian.h
    guid.cc
    guid.h
    location.cc
    location.h
    logging.cc
    logging.h
    macros_magic.h
    power_controller.h
    power_controller_win.cc
    process_handle.cc
    process_handle.h
    scoped_clear_last_error.cc
    scoped_clear_last_error.h
    session_id.cc
    session_id.h
    settings.cc
    settings.h
    smbios.h
    smbios_parser.cc
    smbios_parser.h
    smbios_reader.h
    smbios_reader_win.cc
    stl_util.h
    stream_converter.h
    sys_info.cc
    sys_info.h
    sys_info_win.cc
    system_error.cc
    system_error.h
    system_time.cc
    system_time.h
    task_runner.cc
    task_runner.h
    version.cc
    version.h
    waitable_event.cc
    waitable_event.h
    waitable_timer.cc
    waitable_timer.h
    xml_sax_writer.cc
    xml_sax_writer.h
    xml_settings.cc
    xml_settings.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    base64_unittest.cc
    bitset_unittest.cc
    guid_unittest.cc
    scoped_clear_last_error_unittest.cc
    stl_util_unittest.cc
    stream_converter_unittest.cc
    version_unittest.cc
    xml_settings_unittest.cc)

list(APPEND SOURCE_BASE_FILES
    files/base_paths.cc
    files/base_paths.h
    files/file_path_watcher.cc
    files/file_path_watcher.h
    files/file_path_watcher_win.cc
    files/file_util.cc
    files/file_util.h
    files/scoped_temp_file.cc
    files/scoped_temp_file.h)

list(APPEND SOURCE_BASE_MEMORY
    memory/aligned_memory.cc
    memory/aligned_memory.h
    memory/byte_array.cc
    memory/byte_array.h
    memory/scalable_queue.h
    memory/typed_buffer.h)

list(APPEND SOURCE_BASE_MEMORY_UNIT_TESTS
    memory/aligned_memory_unittest.cc)

list(APPEND SOURCE_BASE_MESSAGE_LOOP
    message_loop/message_loop.cc
    message_loop/message_loop.h
    message_loop/message_loop_task_runner.cc
    message_loop/message_loop_task_runner.h
    message_loop/message_pump.h
    message_loop/message_pump_asio.cc
    message_loop/message_pump_asio.h
    message_loop/message_pump_default.cc
    message_loop/message_pump_default.h
    message_loop/message_pump_dispatcher.h
    message_loop/message_pump_win.cc
    message_loop/message_pump_win.h
    message_loop/pending_task.cc
    message_loop/pending_task.h)

list(APPEND SOURCE_BASE_STRINGS
    strings/string_number_conversions.cc
    strings/string_number_conversions.h
    strings/string_printf.cc
    strings/string_printf.h
    strings/string_split.cc
    strings/string_split.h
    strings/string_util.cc
    strings/string_util.h
    strings/string_util_constants.cc
    strings/string_util_constants.h
    strings/string_util_win.cc
    strings/unicode.cc
    strings/unicode.h)

list(APPEND SOURCE_BASE_STRINGS_UNIT_TESTS
    strings/string_number_conversions_unittest.cc
    strings/string_printf_unittest.cc
    strings/string_split_unittest.cc)

list(APPEND SOURCE_BASE_THREADING
    threading/simple_thread.cc
    threading/simple_thread.h
    threading/thread.cc
    threading/thread.h
    threading/thread_checker.cc
    threading/thread_checker.h)

list(APPEND SOURCE_BASE_WIN
    win/desktop.cc
    win/desktop.h
    win/drive_enumerator.cc
    win/drive_enumerator.h
    win/file_version_info.cc
    win/file_version_info.h
    win/message_window.cc
    win/message_window.h
    win/object_watcher.cc
    win/object_watcher.h
    win/printer_enumerator.cc
    win/printer_enumerator.h
    win/process.cc
    win/process.h
    win/process_util.cc
    win/process_util.h
    win/registry.cc
    win/registry.h
    win/resource_util.cc
    win/resource_util.h
    win/scoped_clipboard.cc
    win/scoped_clipboard.h
    win/scoped_com_initializer.cc
    win/scoped_com_initializer.h
    win/scoped_gdi_object.h
    win/scoped_hdc.h
    win/scoped_hglobal.h
    win/scoped_impersonator.cc
    win/scoped_impersonator.h
    win/scoped_local.h
    win/scoped_object.h
    win/scoped_select_object.h
    win/scoped_thread_desktop.cc
    win/scoped_thread_desktop.h
    win/scoped_user_object.h
    win/scoped_wts_memory.h
    win/security_helpers.cc
    win/security_helpers.h
    win/service.cc
    win/service.h
    win/service_controller.cc
    win/service_controller.h
    win/session_enumerator.cc
    win/session_enumerator.h
    win/session_info.cc
    win/session_info.h
    win/session_status.cc
    win/session_status.h
    win/session_watcher.cc
    win/session_watcher.h
    win/windows_version.cc
    win/windows_version.h)

list(APPEND SOURCE_BASE_WIN_UNIT_TESTS
    win/registry_unittest.cc
    win/scoped_object_unittest.cc)

source_group("" FILES ${SOURCE_BASE})
source_group("" FILES ${SOURCE_BASE_UNIT_TESTS})
source_group(files FILES ${SOURCE_BASE_FILES})
source_group(memory FILES ${SOURCE_BASE_MEMORY})
source_group(memory FILES ${SOURCE_BASE_MEMORY_UNIT_TESTS})
source_group(message_loop FILES ${SOURCE_BASE_MESSAGE_LOOP})
source_group(strings FILES ${SOURCE_BASE_STRINGS})
source_group(strings FILES ${SOURCE_BASE_STRINGS_UNIT_TESTS})
source_group(threading FILES ${SOURCE_BASE_THREADING})
source_group(win FILES ${SOURCE_BASE_WIN})
source_group(win FILES ${SOURCE_BASE_WIN_UNIT_TESTS})

add_library(aspia_base STATIC
    ${SOURCE_BASE}
    ${SOURCE_BASE_FILES}
    ${SOURCE_BASE_MEMORY}
    ${SOURCE_BASE_MESSAGE_LOOP}
    ${SOURCE_BASE_STRINGS}
    ${SOURCE_BASE_THREADING}
    ${SOURCE_BASE_WIN})
target_link_libraries(aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_base_tests
        ${SOURCE_BASE_UNIT_TESTS}
        ${SOURCE_BASE_MEMORY_UNIT_TESTS}
        ${SOURCE_BASE_STRINGS_UNIT_TESTS}
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

