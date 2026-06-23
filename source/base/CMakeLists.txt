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
    pch.h
    power_controller.cc
    power_controller.h
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

if (UNIX)
    include(posix/sources.cmake)
endif()

if (WIN32)
    include(audio/win/sources.cmake)
    include(desktop/win/sources.cmake)
    include(win/sources.cmake)
endif()

if (LINUX)
    include(audio/linux/sources.cmake)
    include(desktop/wayland/sources.cmake)
    include(desktop/x11/sources.cmake)
    include(linux/sources.cmake)
    include(x11/sources.cmake)
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
    source_group(desktop\\win FILES ${SOURCE_BASE_DESKTOP_WIN} ${SOURCE_BASE_DESKTOP_WIN_TESTS})
    source_group(posix FILES ${SOURCE_BASE_POSIX})
    source_group(win FILES ${SOURCE_BASE_WIN} ${SOURCE_BASE_WIN_TESTS})
endif()

if (LINUX)
    source_group(audio\\linux FILES ${SOURCE_BASE_AUDIO_LINUX})
    source_group(desktop\\wayland FILES ${SOURCE_BASE_DESKTOP_WAYLAND})
    source_group(desktop\\x11 FILES ${SOURCE_BASE_DESKTOP_X11})
    source_group(linux FILES ${SOURCE_BASE_LINUX})
    source_group(posix FILES ${SOURCE_BASE_POSIX})
    source_group(x11 FILES ${SOURCE_BASE_X11})
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
    ${SOURCE_BASE_DESKTOP_WAYLAND}
    ${SOURCE_BASE_DESKTOP_WIN}
    ${SOURCE_BASE_DESKTOP_X11}
    ${SOURCE_BASE_FILES}
    ${SOURCE_BASE_IPC}
    ${SOURCE_BASE_LINUX}
    ${SOURCE_BASE_MAC}
    ${SOURCE_BASE_NET}
    ${SOURCE_BASE_PEER}
    ${SOURCE_BASE_POSIX}
    ${SOURCE_BASE_SQL}
    ${SOURCE_BASE_WIN}
    ${SOURCE_BASE_X11})

set_property(TARGET aspia_base PROPERTY AUTOMOC ON)
set_property(TARGET aspia_base PROPERTY AUTOUIC ON)
set_property(TARGET aspia_base PROPERTY AUTORCC ON)
qt_import_plugins(aspia_base INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

target_precompile_headers(aspia_base PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:pch.h>")

if (WIN32)
    set(BASE_PLATFORM_LIBS mfuuid strmiids wevtapi)
endif()

if (LINUX)
    set(BASE_PLATFORM_LIBS ${X11_LIB} ${XFIXES_LIB} stdc++fs ${CMAKE_DL_LIBS})

    # PipeWire, EGL/GBM and libdrm are loaded dynamically (see base/desktop/wayland), so only their
    # headers are needed at build time, not the libraries.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(LIBDRM REQUIRED libdrm)
    target_include_directories(aspia_base PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${LIBDRM_INCLUDE_DIRS})
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
    ${SOURCE_BASE_DESKTOP_WIN_TESTS}
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

target_precompile_headers(aspia_base_tests PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:pch.h>")

add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
