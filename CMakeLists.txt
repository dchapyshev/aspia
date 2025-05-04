#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

cmake_minimum_required(VERSION 3.21.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(aspia)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/source/cmake)

if (WIN32)
    include(FindWDK)
endif()

set(ASPIA_VERSION_MAJOR 2)
set(ASPIA_VERSION_MINOR 8)
set(ASPIA_VERSION_PATCH 0)

# On CI installed dir could be outside of vcpkg root
if ("$ENV{VCPKG_INSTALLED_DIR}" STREQUAL "")
    set(VCPKG_INSTALLED_DIR $ENV{VCPKG_ROOT}/installed)
    message(STATUS "VCPKG_INSTALLED_DIR -> redefine: ${VCPKG_INSTALLED_DIR}")
else()
    set(VCPKG_INSTALLED_DIR $ENV{VCPKG_INSTALLED_DIR})
    message(STATUS "VCPKG_INSTALLED_DIR -> external: ${VCPKG_INSTALLED_DIR}")
endif()

add_definitions(-DASPIA_VERSION_MAJOR=${ASPIA_VERSION_MAJOR}
                -DASPIA_VERSION_MINOR=${ASPIA_VERSION_MINOR}
                -DASPIA_VERSION_PATCH=${ASPIA_VERSION_PATCH})

if (UNIX AND NOT APPLE)
    message(STATUS "Linux detected")
    set(LINUX TRUE)
endif()

if (APPLE)
    message(STATUS "MacOSX detected")
endif()

if (WIN32)
    message(STATUS "Windows detected")
endif()

set(CMAKE_SKIP_BUILD_RPATH ON)

find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets Xml)
find_package(Qt5LinguistTools)
find_package(asio CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(libyuv CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(unofficial-libvpx CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

if (WIN32)
    find_package(Qt5WinExtras REQUIRED)
    find_package(WDK REQUIRED)
endif()

if (LINUX)
    find_library(XFIXES_LIB NAMES libXfixes Xfixes REQUIRED)
    message(STATUS "XFixes library: ${XFIXES_LIB}")

    find_library(XDAMAGE_LIB NAMES libXdamage Xdamage REQUIRED)
    message(STATUS "XDamage library: ${XDAMAGE_LIB}")

    find_library(XRANDR_LIB NAMES libXrandr Xrandr REQUIRED)
    message(STATUS "XRandr library: ${XRANDR_LIB}")

    find_library(XTST_LIB NAMES libXtst Xtst REQUIRED)
    message(STATUS "XTst library: ${XTST_LIB}")

    find_library(XI_LIB NAMES libXi Xi REQUIRED)
    message(STATUS "Xi library: ${XI_LIB}")

    find_library(X11_LIB NAMES libX11 X11 REQUIRED)
    message(STATUS "X11 library: ${X11_LIB}")

    find_library(XEXT_LIB NAMES libXext Xext REQUIRED)
    message(STATUS "Xext library: ${XEXT_LIB}")
endif()

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")
    find_library(FOUNDATION_LIB Foundation)
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_program(CODESIGN_BIN NAMES codesign)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

add_subdirectory(source)
