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

project(aspia)

# Detecting Linux.
if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# Qt configuration.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets Xml)
find_package(Qt5LinguistTools)
find_package(asio REQUIRED)
find_package(GTest REQUIRED)
find_package(libyuv REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(unofficial-libvpx REQUIRED)
find_package(unofficial-sqlite3 REQUIRED)
find_package(zstd REQUIRED)

find_path(RAPIDXML_INCLUDE_DIRS "rapidxml/rapidxml.hpp")

if (WIN32)
    find_package(Qt5WinExtras REQUIRED)

    # Target version.
    add_definitions(-DNTDDI_VERSION=0x06010000
                    -D_WIN32_WINNT=0x0601
                    -D_WIN32_WINDOWS=_WIN32_WINNT
                    -DWINVER=_WIN32_WINNT
                    -D_WIN32_IE=0x0800
                    -DPSAPI_VERSION=2)

    # Other definitions.
    add_definitions(-D_UNICODE
                    -DUNICODE
                    -DWIN32_LEAN_AND_MEAN
                    -DNOMINMAX)
endif()

if (UNIX)
    set(INSTALLED_DIR "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

    find_library(ICUUC_RELEASE NAMES icuuc PATHS "${INSTALLED_DIR}/lib" NO_DEFAULT_PATH)
    find_library(ICUUC_DEBUG NAMES icuuc PATHS "${INSTALLED_DIR}/debug/lib" NO_DEFAULT_PATH)

    find_library(ICUDATA_RELEASE NAMES icudata PATHS "${INSTALLED_DIR}/lib" NO_DEFAULT_PATH)
    find_library(ICUDATA_DEBUG NAMES icudata PATHS "${INSTALLED_DIR}/debug/lib" NO_DEFAULT_PATH)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

# For Asio.
add_definitions(-DASIO_STANDALONE
                -DASIO_NO_DEPRECATED
                -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

# For RapidJSON.
add_definitions(-DRAPIDJSON_SSE2)

# For Qt.
add_definitions(-DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})

set(QT_COMMON_LIBS
    Qt5::Core
    Qt5::CorePrivate
    Qt5::QGenericEnginePlugin
    Qt5::Gui
    Qt5::GuiPrivate
    Qt5::QICOPlugin
    Qt5::Network
    Qt5::NetworkPrivate
    Qt5::PrintSupport
    Qt5::PrintSupportPrivate
    Qt5::Widgets
    Qt5::WidgetsPrivate
    Qt5::Xml
    Qt5::XmlPrivate)

if (WIN32)
    set(QT_PLATFORM_LIBS
        Qt5::WinMain
        Qt5::WinExtras
        Qt5::WinExtrasPrivate
        Qt5::QWindowsIntegrationPlugin
        Qt5::QWindowsVistaStylePlugin
        Qt5::QWindowsPrinterSupportPlugin)
endif()

set(THIRD_PARTY_LIBS
    ${Protobuf_LITE_LIBRARIES}
    libzstd
    OpenSSL::Crypto
    OpenSSL::SSL
    modp_b64
    unofficial::libvpx::libvpx
    x11region
    yuv)

# C++ compliller flags.
set(CMAKE_CXX_STANDARD 17)

if (MSVC)
    # C++ compliller flags.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /W3 /MP /arch:SSE2")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MP /W3")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /GL /W3 /MP /arch:SSE2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MP /W3")

    # Linker flags.
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

    # Static runtime library.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

add_subdirectory(base)
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(console)
add_subdirectory(host)
add_subdirectory(proto)
add_subdirectory(qt_base)
add_subdirectory(relay)
add_subdirectory(router)
add_subdirectory(third_party)
