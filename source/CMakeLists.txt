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

cmake_minimum_required(VERSION 3.12.1)

option(BUILD_UNIT_TESTS "Build unit tests" ON)
option(USE_PCG_GENERATOR "Using PCG random generator" ON)

set(CMAKE_SYSTEM_VERSION 7.0 CACHE TYPE INTERNAL FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 8.1 CACHE TYPE INTERNAL FORCE)

set(ASPIA_THIRD_PARTY_DIR "$ENV{ASPIA_THIRD_PARTY_DIR}")

project(aspia)

# Qt configuration.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "${ASPIA_THIRD_PARTY_DIR}/qt;${ASPIA_THIRD_PARTY_DIR}/protobuf")

find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets WinExtras Xml)
find_package(Qt5LinguistTools)
find_package(Protobuf REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

add_definitions(-D_UNICODE
                -DUNICODE
                -DNTDDI_VERSION=0x06010000
                -D_WIN32_WINNT=0x0601
                -D_WIN32_WINDOWS=_WIN32_WINNT
                -DWINVER=_WIN32_WINNT
                -D_WIN32_IE=0x0800
                -DPSAPI_VERSION=2
                -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
                -D_CRT_SECURE_NO_WARNINGS
                -DNOMINMAX
                -DHAS_QT
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER
                -DCLIENT_IMPLEMENTATION
                -DHOST_IMPLEMENTATION)

if (USE_PCG_GENERATOR)
    add_definitions(-DUSE_PCG_GENERATOR)
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
    ${ASPIA_THIRD_PARTY_DIR}/googletest/include
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/include
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/include
    ${ASPIA_THIRD_PARTY_DIR}/openssl/include
    ${ASPIA_THIRD_PARTY_DIR}/pcg-cpp/include
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/include
    ${ASPIA_THIRD_PARTY_DIR}/zstd/include)

link_directories(
    ${ASPIA_THIRD_PARTY_DIR}/googletest/lib
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/lib
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/lib
    ${ASPIA_THIRD_PARTY_DIR}/openssl/lib
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/platforms
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/printsupport
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/styles
    ${ASPIA_THIRD_PARTY_DIR}/zstd/lib)

set(THIRD_PARTY_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::PrintSupport
    Qt5::Widgets
    Qt5::WinMain
    Qt5::WinExtras
    Qt5::Xml
    debug Qt5AccessibilitySupportd
    debug Qt5EventDispatcherSupportd
    debug Qt5FontDatabaseSupportd
    debug Qt5ThemeSupportd
    debug Qt5WindowsUIAutomationSupportd
    debug libprotobuf-lited
    debug vpxmtd
    debug libyuvd
    debug qtfreetyped
    debug qtharfbuzzd
    debug qtlibpngd
    debug qtpcre2d
    debug qwindowsd
    debug qwindowsvistastyled
    debug windowsprintersupportd
    debug zstdd
    optimized Qt5AccessibilitySupport
    optimized Qt5EventDispatcherSupport
    optimized Qt5FontDatabaseSupport
    optimized Qt5ThemeSupport
    optimized Qt5WindowsUIAutomationSupport
    optimized libprotobuf-lite
    optimized vpxmt
    optimized libyuv
    optimized qtfreetype
    optimized qtharfbuzz
    optimized qtlibpng
    optimized qtpcre2
    optimized qwindows
    optimized qwindowsvistastyle
    optimized windowsprintersupport
    optimized zstd
    crypt32
    dwmapi
    dxgi
    d3d11
    imm32
    iphlpapi
    libcrypto
    libssl
    mpr
    netapi32
    sas
    setupapi
    shlwapi
    userenv
    uxtheme
    version
    winmm
    ws2_32
    wtsapi32)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /Oy /GL /MT /MP /arch:SSE2 /fp:fast /wd4146")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP /wd4146")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    enable_testing()
endif()

add_subdirectory(base)
add_subdirectory(client)
add_subdirectory(codec)
add_subdirectory(common)
add_subdirectory(console)
add_subdirectory(crypto)
add_subdirectory(desktop)
add_subdirectory(host)
add_subdirectory(ipc)
add_subdirectory(net)
add_subdirectory(proto)
add_subdirectory(updater)
