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

cmake_minimum_required(VERSION 2.8.11)

option(BUILD_UNIT_TESTS "Build unit tests" ON)

set(CMAKE_SYSTEM_VERSION 7.0 CACHE TYPE INTERNAL FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 8.1 CACHE TYPE INTERNAL FORCE)

set(ASPIA_THIRD_PARTY_DIR "$ENV{ASPIA_THIRD_PARTY_DIR}")

project(aspia)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "${ASPIA_THIRD_PARTY_DIR}/qt;${ASPIA_THIRD_PARTY_DIR}/protobuf")

find_package(Qt5 REQUIRED Core Gui Network Widgets WinExtras Xml)
find_package(Qt5LinguistTools)
find_package(Protobuf REQUIRED)

include(aspia_core.cmake)

source_group(base FILES ${SOURCE_BASE})
source_group(base FILES ${SOURCE_BASE_UNIT_TESTS})
source_group(base\\win FILES ${SOURCE_BASE_WIN})
source_group(client FILES ${SOURCE_CLIENT})
source_group(client\\ui FILES ${SOURCE_CLIENT_UI})
source_group(codec FILES ${SOURCE_CODEC})
source_group(console FILES ${SOURCE_CONSOLE})
source_group(crypto FILES ${SOURCE_CRYPTO})
source_group(crypto FILES ${SOURCE_CRYPTO_UNIT_TESTS})
source_group(desktop_capture FILES ${SOURCE_DESKTOP_CAPTURE})
source_group(desktop_capture FILES ${SOURCE_DESKTOP_CAPTURE_UNIT_TESTS})
source_group(desktop_capture\\win FILES ${SOURCE_DESKTOP_CAPTURE_WIN})
source_group(host FILES ${SOURCE_HOST})
source_group(host\\ui FILES ${SOURCE_HOST_UI})
source_group(host\\win FILES ${SOURCE_HOST_WIN})
source_group(ipc FILES ${SOURCE_IPC})
source_group(network FILES ${SOURCE_NETWORK})
source_group(protocol FILES ${SOURCE_PROTOCOL})
source_group(share FILES ${SOURCE_SHARE})
source_group(system_info FILES ${SOURCE_SYSTEM_INFO})
source_group(system_info\\parser FILES ${SOURCE_SYSTEM_INFO_PARSER})
source_group(system_info\\protocol FILES ${SOURCE_SYSTEM_INFO_PROTOCOL})
source_group(system_info\\serializer FILES ${SOURCE_SYSTEM_INFO_SERIALIZER})
source_group(system_info\\ui FILES ${SOURCE_SYSTEM_INFO_UI})
source_group("" FILES ${SOURCE})

protobuf_generate_cpp(PROTOCOL_CC_FILES PROTOCOL_H_FILES ${SOURCE_PROTOCOL} ${SOURCE_SYSTEM_INFO_PROTOCOL})
set_source_files_properties(${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES} PROPERTIES SKIP_AUTOGEN ON)

add_definitions(-D_UNICODE
                -DNTDDI_VERSION=0x06010000
                -D_WIN32_WINNT=0x0601
                -D_WIN32_WINDOWS=_WIN32_WINNT
                -DWINVER=_WIN32_WINNT
                -D_WIN32_IE=0x0800
                -DPSAPI_VERSION=2
                -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
                -D_CRT_SECURE_NO_WARNINGS
                -DCORE_IMPLEMENTATION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /Oy /GL /MT /MP /arch:SSE2 /fp:fast /wd4146")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP /wd4146")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

add_library(aspia_core SHARED ${ALL_SOURCES} ${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES})
target_link_libraries(aspia_core ${THIRD_PARTY_LIBS})

add_executable(aspia_console ${PROJECT_SOURCE_DIR}/console/entry_point.cc ${PROJECT_SOURCE_DIR}/console/console.rc)
set_target_properties(aspia_console PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(aspia_console aspia_core)

add_executable(aspia_host_config ${PROJECT_SOURCE_DIR}/host/host_config_entry_point.cc ${PROJECT_SOURCE_DIR}/host/host_config.rc)
set_target_properties(aspia_host_config PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_config PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_config aspia_core)

add_executable(aspia_host_service ${PROJECT_SOURCE_DIR}/host/win/host_service_entry_point.cc ${PROJECT_SOURCE_DIR}/host/win/host_service.rc)
target_link_libraries(aspia_host_service aspia_core)

add_executable(aspia_host ${PROJECT_SOURCE_DIR}/host/win/host_entry_point.cc ${PROJECT_SOURCE_DIR}/host/win/host.rc)
set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host aspia_core)

add_subdirectory(client)
add_subdirectory(console)
add_subdirectory(host)

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_unit_tests ${ALL_SOURCES_WITH_UNIT_TESTS} ${PROTOCOL_CC_FILES} ${PROTOCOL_H_FILES})
    target_link_libraries(aspia_unit_tests
        ${THIRD_PARTY_LIBS}
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind)

    enable_testing()
    add_test(NAME aspia_unit_tests COMMAND aspia_unit_tests)
endif()
