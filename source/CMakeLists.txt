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

include(GNUInstallDirs)

# Qt configuration.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_AUTORCC_OPTIONS -compress 9 -threshold 5)

if (WIN32)
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

# For Asio.
add_definitions(-DASIO_STANDALONE
                -DASIO_NO_DEPRECATED
                -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)

# For Qt.
add_definitions(#-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_CAST_TO_ASCII
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_USE_QSTRINGBUILDER)

set(QT_COMMON_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Qt6::Svg
    Qt6::Xml)

if (NOT ANDROID)
    list(APPEND QT_COMMON_LIBS
        Qt6::DeviceDiscoverySupportPrivate
        Qt6::FbSupportPrivate
        Qt6::QICOPlugin
        Qt6::PrintSupport Qt6::PrintSupport_resources_1 Qt6::PrintSupport_resources_2
        Qt6::Widgets_resources_1 Qt6::Widgets_resources_2)
endif()

if (WIN32)
    set(QT_PLATFORM_LIBS Qt6::QWindowsIntegrationPlugin)
endif()

if (LINUX)
    set(QT_PLATFORM_LIBS
        Qt6::XcbQpaPrivate
        Qt6::DBus)
endif()

if (APPLE)
    set(QT_PLATFORM_LIBS
        Qt6::OpenGL
        Qt6::QMacStylePlugin)
endif()

set(THIRD_PARTY_LIBS
    CURL::libcurl
    protobuf::libprotobuf-lite
    zstd::libzstd_static
    OpenSSL::Crypto
    OpenSSL::SSL
    Opus::opus
    ${OPENH264_LIBRARY}
    unofficial::libwebm::libwebm
    unofficial::libvpx::libvpx
    unofficial::enet::enet
    miniupnpc::miniupnpc
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    unofficial::sqlite3::sqlite3
    vterm
    yuv)

if (LINUX OR APPLE)
    list(APPEND THIRD_PARTY_LIBS ICU::uc)
endif()

include_directories(${PROJECT_SOURCE_DIR}/source ${PROJECT_BINARY_DIR}/source)

# C++ compliller flags.
set(CMAKE_CXX_STANDARD 20)

message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_LIBRARY_OUTPUT_DIRECTORY: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_COMMAND: ${CMAKE_COMMAND}")
message(STATUS "CMAKE_MAKE_PROGRAM: ${CMAKE_MAKE_PROGRAM}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    message(STATUS "DEBUG build detected")
else()
    message(STATUS "RELEASE build detected")
endif()

if (MSVC)
    # C++ compliller flags.
    # /Zi enables PDB with source and line info so the crash handler can symbolize frames.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /MP /Zi")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /MP")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /W3 /MP /Zi")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /W3 /MP")

    if (${CMAKE_CL_64} EQUAL 0)
        message(STATUS "x86 compiller detected")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /arch:SSE2")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /arch:SSE2")
    else()
        message(STATUS "x64 compiller detected")
    endif()

    # Linker flags.
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /IGNORE:4099")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /IGNORE:4099")
endif()

if (LINUX)
    # C++ compliller flags.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")

    # C compiller flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 12 on aarch64 reads a field before a write to that same field when the two are reached
    # through different pointer types, so a queue reports the size it had before the removal.
    # C++ compiler flags.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-strict-aliasing")

    # C compiler flags.
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-strict-aliasing")
endif()

# Compiller flags.
message(STATUS "CXX compiller version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CXX flags (release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CXX flags (debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C compiller version: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C flags (release): ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "C flags (debug): ${CMAKE_C_FLAGS_DEBUG}")

# Linker flags.
message(STATUS "Linker flags (shared release): ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (shared debug): ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker flags (static release): ${CMAKE_STATIC_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (static debug): ${CMAKE_STATIC_LINKER_FLAGS_DEBUG}")
message(STATUS "Linker flags (exe release): ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
message(STATUS "Linker flags (exe debug): ${CMAKE_EXE_LINKER_FLAGS_DEBUG}")

# Get git branch info.
execute_process(COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_CURRENT_BRANCH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get git commit hash.
execute_process(COMMAND git rev-parse HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get git commit count.
execute_process(COMMAND git rev-list --count HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_COMMIT_COUNT
                OUTPUT_STRIP_TRAILING_WHITESPACE)

if (NOT GIT_COMMIT_COUNT GREATER 0)
    message(STATUS "Failed to determine the number of commits")
    set(GIT_COMMIT_COUNT 0)
endif()

# Add git branch and commit hash to definitions.
add_definitions(-DGIT_CURRENT_BRANCH=\"${GIT_CURRENT_BRANCH}\"
                -DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"
                -DGIT_COMMIT_COUNT=${GIT_COMMIT_COUNT})

message(STATUS "Git branch: ${GIT_CURRENT_BRANCH}")
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
message(STATUS "Git commit count: ${GIT_COMMIT_COUNT}")

if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ASPIA_HOST_ONLY TRUE)
    message(STATUS "Host-only build")
endif()

add_subdirectory(tools)
add_subdirectory(base)
add_subdirectory(common)
add_subdirectory(proto)
add_subdirectory(third_party)

if (NOT ASPIA_HOST_ONLY)
    add_subdirectory(client)
endif()

if (NOT ANDROID)
    add_subdirectory(drivers)

    if (NOT ASPIA_HOST_ONLY)
        add_subdirectory(relay)
        add_subdirectory(router)
    endif()
endif()

if (WIN32 OR LINUX OR APPLE OR ANDROID)
    add_subdirectory(host)
endif()
