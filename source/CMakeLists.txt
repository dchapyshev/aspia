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

cmake_minimum_required(VERSION 3.16.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(BUILD_UNIT_TESTS "Build unit tests" ON)
option(USE_TBB "Using Intel TBB" ON)

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

# Target version.
add_definitions(-DNTDDI_VERSION=0x06010000
                -D_WIN32_WINNT=0x0601
                -D_WIN32_WINDOWS=_WIN32_WINNT
                -DWINVER=_WIN32_WINNT
                -D_WIN32_IE=0x0800
                -DPSAPI_VERSION=2)

# For Asio.
add_definitions(-DASIO_STANDALONE
                -DASIO_NO_DEPRECATED)

# For Qt.
add_definitions(-DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER)

# For Windows.
add_definitions(-D_UNICODE
                -DUNICODE
                -DWIN32_LEAN_AND_MEAN
                -DNOMINMAX)

if (USE_TBB)
    add_definitions(-DUSE_TBB)
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
    ${ASPIA_THIRD_PARTY_DIR}/asio/include
    ${ASPIA_THIRD_PARTY_DIR}/googletest/include
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/include
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/include
    ${ASPIA_THIRD_PARTY_DIR}/openssl/include
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/include
    ${ASPIA_THIRD_PARTY_DIR}/rapidxml/include
    ${ASPIA_THIRD_PARTY_DIR}/tbb/include
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
    ${ASPIA_THIRD_PARTY_DIR}/tbb/lib
    ${ASPIA_THIRD_PARTY_DIR}/zstd/lib)

set(QT_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::PrintSupport
    Qt5::Widgets
    Qt5::WinMain
    Qt5::WinExtras
    Qt5::Xml
    Qt5::QWindowsIntegrationPlugin
    Qt5::QWindowsVistaStylePlugin
    Qt5::QWindowsPrinterSupportPlugin
    debug Qt5AccessibilitySupportd
    debug Qt5EventDispatcherSupportd
    debug Qt5FontDatabaseSupportd
    debug Qt5ThemeSupportd
    debug Qt5WindowsUIAutomationSupportd
    debug qtfreetyped
    debug qtharfbuzzd
    debug qtlibpngd
    debug qtpcre2d
    optimized Qt5AccessibilitySupport
    optimized Qt5EventDispatcherSupport
    optimized Qt5FontDatabaseSupport
    optimized Qt5ThemeSupport
    optimized Qt5WindowsUIAutomationSupport
    optimized qtfreetype
    optimized qtharfbuzz
    optimized qtlibpng
    optimized qtpcre2)

set(WINDOWS_LIBS
    crypt32
    dwmapi
    dxgi
    d3d11
    imm32
    iphlpapi
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

set(THIRD_PARTY_LIBS
    debug libprotobuf-lited
    debug vpxmtd
    debug yuv_debug
    debug tbb_debug
    debug zstd_static_debug
    optimized libprotobuf-lite
    optimized vpxmt
    optimized yuv
    optimized tbb
    optimized zstd_static
    libcrypto
    libssl
    modp_b64
    x11region)

# C++ compliller flags.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /MP /arch:SSE2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP")

# C compiller flags.
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /MP /arch:SSE2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MP")

# Linker flags.
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

# Static runtime library.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
add_subdirectory(proxy)
add_subdirectory(qt_base)
add_subdirectory(router)
add_subdirectory(third_party)
add_subdirectory(updater)
