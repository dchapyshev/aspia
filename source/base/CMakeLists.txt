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

list(APPEND SOURCE_BASE
    aligned_memory.cc
    aligned_memory.h
    applications_reader.cc
    applications_reader.h
    asio_event_dispatcher.cc
    asio_event_dispatcher.h
    bitset.h
    bucket_pool.h
    core_application.cc
    core_application.h
    cpuid_util.cc
    cpuid_util.h
    crash_handler.cc
    crash_handler.h
    debug.cc
    debug.h
    edid.cc
    edid.h
    gui_application.cc
    gui_application.h
    license_reader.cc
    license_reader.h
    location.cc
    location.h
    logging.cc
    logging.h
    logging_file.cc
    logging_file.h
    numeric_utils.h
    power_controller.cc
    power_controller.h
    power_save_blocker.cc
    power_save_blocker.h
    process_util.cc
    process_util.h
    scoped_clear_last_error.cc
    scoped_clear_last_error.h
    scoped_qpointer.h
    security_log.cc
    security_log.h
    serialization.cc
    serialization.h
    service_controller.cc
    service_controller.h
    session_id.cc
    session_id.h
    shared_pointer.h
    smbios.h
    smbios_parser.cc
    smbios_parser.h
    string_util.cc
    string_util.h
    sys_info.cc
    sys_info.h
    system_error.cc
    system_error.h
    thread.cc
    thread.h
    translations.cc
    translations.h
    version_constants.cc
    version_constants.h
    xml_settings.cc
    xml_settings.h)

if (WIN32)
    list(APPEND SOURCE_BASE
        core_service.h
        core_service_win.cc
        service_controller_win.cc
        service_controller_win.h
        sys_info_win.cc)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE
        core_service.h
        core_service_posix.cc
        service_controller_systemd.cc
        service_controller_systemd.h
        sys_info_linux.cc)
endif()

if (APPLE)
    list(APPEND SOURCE_BASE
        core_service.h
        core_service_posix.cc
        sys_info_mac.mm)
endif()

if (ANDROID)
    list(APPEND SOURCE_BASE
        sys_info_android.cc)
endif()

list(APPEND SOURCE_BASE_TESTS
    aligned_memory_unittest.cc
    asio_event_dispatcher_unittest.cc
    bitset_unittest.cc
    bucket_pool_unittest.cc
    edid_unittest.cc
    numeric_utils_unittest.cc
    scoped_clear_last_error_unittest.cc
    serialization_unittest.cc
    shared_pointer_unittest.cc
    string_util_unittest.cc
    xml_settings_unittest.cc
    tests_main.cc)

include(audio/sources.cmake)
include(codec/sources.cmake)
include(crypto/sources.cmake)
include(desktop/sources.cmake)
include(files/sources.cmake)
include(ipc/sources.cmake)
include(net/sources.cmake)
include(peer/sources.cmake)
include(sql/sources.cmake)

if (WIN32)
    include(audio/win/sources.cmake)
    include(win/sources.cmake)
endif()

if (LINUX)
    include(audio/linux/sources.cmake)
    include(linux/sources.cmake)
endif()

if (APPLE)
    include(mac/sources.cmake)
endif()

source_group("" FILES ${SOURCE_BASE} ${SOURCE_BASE_TESTS})
source_group(audio FILES ${SOURCE_BASE_AUDIO})
source_group(codec FILES ${SOURCE_BASE_CODEC} ${SOURCE_BASE_CODEC_TESTS})
source_group(crypto FILES ${SOURCE_BASE_CRYPTO} ${SOURCE_BASE_CRYPTO_TESTS})
source_group(desktop FILES ${SOURCE_BASE_DESKTOP} ${SOURCE_BASE_DESKTOP_TESTS})
source_group(files FILES ${SOURCE_BASE_FILES} ${SOURCE_BASE_FILES_TESTS})
source_group(ipc FILES ${SOURCE_BASE_IPC})
source_group(net FILES ${SOURCE_BASE_NET} ${SOURCE_BASE_NET_TESTS})
source_group(peer FILES ${SOURCE_BASE_PEER} ${SOURCE_BASE_PEER_TESTS})
source_group(sql FILES ${SOURCE_BASE_SQL} ${SOURCE_BASE_SQL_TESTS})

if (WIN32)
    source_group(audio\\win FILES ${SOURCE_BASE_AUDIO_WIN})
    source_group(win FILES ${SOURCE_BASE_WIN} ${SOURCE_BASE_WIN_TESTS})
endif()

if (LINUX)
    source_group(audio\\linux FILES ${SOURCE_BASE_AUDIO_LINUX})
    source_group(linux FILES ${SOURCE_BASE_LINUX})
endif()

if (APPLE)
    source_group(mac FILES ${SOURCE_BASE_MAC})
endif()

add_library(aspia_base STATIC
    ${SOURCE_BASE}
    ${SOURCE_BASE_AUDIO}
    ${SOURCE_BASE_AUDIO_LINUX}
    ${SOURCE_BASE_AUDIO_WIN}
    ${SOURCE_BASE_CODEC}
    ${SOURCE_BASE_CRYPTO}
    ${SOURCE_BASE_DESKTOP}
    ${SOURCE_BASE_FILES}
    ${SOURCE_BASE_IPC}
    ${SOURCE_BASE_LINUX}
    ${SOURCE_BASE_MAC}
    ${SOURCE_BASE_NET}
    ${SOURCE_BASE_PEER}
    ${SOURCE_BASE_SQL}
    ${SOURCE_BASE_WIN})

set_property(TARGET aspia_base PROPERTY AUTOMOC ON)
set_property(TARGET aspia_base PROPERTY AUTOUIC ON)
set_property(TARGET aspia_base PROPERTY AUTORCC ON)
qt_import_plugins(aspia_base INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

if (WIN32)
    set(BASE_PLATFORM_LIBS mfuuid strmiids wevtapi)
endif()

if (LINUX)
    set(BASE_PLATFORM_LIBS stdc++fs ${CMAKE_DL_LIBS})

    # libwayland-client is linked (the generated xdg-output stubs in base/linux call wl_proxy_* directly);
    # it is already pulled in via the static Qt Wayland plugins. The generated xdg-output client header
    # lives in the base binary directory.
    find_library(WAYLAND_CLIENT_LIB NAMES wayland-client REQUIRED)
    list(APPEND BASE_PLATFORM_LIBS ${WAYLAND_CLIENT_LIB})
    target_include_directories(aspia_base PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

if (ANDROID)
    set(BASE_PLATFORM_LIBS log mediandk oboe::oboe)
endif()

target_link_libraries(aspia_base PRIVATE aspia_proto
    ${THIRD_PARTY_LIBS} ${BASE_PLATFORM_LIBS} ${QT_COMMON_LIBS} ${QT_PLATFORM_LIBS})

if (WIN32)
    set(BASE_TESTS_PLATFORM_LIBS avrt crypt32 dbghelp iphlpapi ws2_32)
endif()

if (LINUX)
    set(BASE_TESTS_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    set(BASE_TESTS_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

add_executable(aspia_base_tests
    ${SOURCE_BASE_TESTS}
    ${SOURCE_BASE_CODEC_TESTS}
    ${SOURCE_BASE_CRYPTO_TESTS}
    ${SOURCE_BASE_DESKTOP_TESTS}
    ${SOURCE_BASE_FILES_TESTS}
    ${SOURCE_BASE_NET_TESTS}
    ${SOURCE_BASE_PEER_TESTS}
    ${SOURCE_BASE_SQL_TESTS}
    ${SOURCE_BASE_WIN_TESTS})
target_link_libraries(aspia_base_tests PRIVATE
    aspia_base
    aspia_proto
    GTest::gtest
    ${BASE_TESTS_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS})
set_property(TARGET aspia_base_tests PROPERTY AUTOMOC ON)
set_property(TARGET aspia_base_tests PROPERTY AUTOUIC ON)
set_property(TARGET aspia_base_tests PROPERTY AUTORCC ON)
qt_import_plugins(aspia_base_tests INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
