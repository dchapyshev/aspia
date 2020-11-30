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

cmake_minimum_required(VERSION 3.17.0)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(aspia)

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

message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Host processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets Xml)
find_package(Qt5LinguistTools)
find_package(asio CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(libyuv CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)
find_package(unofficial-libvpx CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

find_path(RAPIDXML_INCLUDE_DIRS "rapidxml/rapidxml.hpp")

if (WIN32)
    find_package(Qt5WinExtras REQUIRED)
endif()

if (UNIX)
    find_package(ICU REQUIRED uc dt)
endif()

if (LINUX)
    find_library(XFIXES_LIB NAMES libXfixes Xfixes REQUIRED)
    message(STATUS "XFixes library: ${XFIXES_LIB}")
endif()

if (APPLE)
    find_library(FOUNDATION_LIB Foundation)
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

add_subdirectory(source)
