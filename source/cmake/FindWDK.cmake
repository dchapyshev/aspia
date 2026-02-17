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

file(GLOB WDK_NTDDK_FILES "C:/Program Files*/Windows Kits/*/Include/*/um/avrt.h")

if(WDK_NTDDK_FILES)
    list(SORT WDK_NTDDK_FILES COMPARE NATURAL)
    list(GET WDK_NTDDK_FILES -1 WDK_LATEST_NTDDK_FILE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WDK REQUIRED_VARS WDK_LATEST_NTDDK_FILE)

if (NOT WDK_LATEST_NTDDK_FILE)
    return()
endif()

get_filename_component(WDK_ROOT ${WDK_LATEST_NTDDK_FILE} DIRECTORY)
get_filename_component(WDK_ROOT ${WDK_ROOT} DIRECTORY)
get_filename_component(WDK_VERSION ${WDK_ROOT} NAME)
get_filename_component(WDK_WDF_ROOT ${WDK_ROOT} DIRECTORY)

if (NOT DEFINED IDDCX_VERSION_MAJOR)
    set(IDDCX_VERSION_MAJOR 1)
endif()

if (NOT DEFINED IDDCX_VERSION_MINOR)
    set(IDDCX_VERSION_MINOR 4)
endif()

if (NOT DEFINED IDDCX_MINIMUM_VERSION_REQUIRED)
    set(IDDCX_MINIMUM_VERSION_REQUIRED 4)
endif()

set(WDK_COMPILE_DEFINITIONS
    "WINNT=1;"
    "UMDF_DRIVER;"
    "IDDCX_VERSION_MAJOR=${IDDCX_VERSION_MAJOR};"
    "IDDCX_VERSION_MINOR=${IDDCX_VERSION_MINOR};"
    "IDDCX_MINIMUM_VERSION_REQUIRED=${IDDCX_MINIMUM_VERSION_REQUIRED}")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(APPEND WDK_COMPILE_DEFINITIONS "_X86_=1;i386=1;_ATL_NO_WIN_SUPPORT")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    list(APPEND WDK_COMPILE_DEFINITIONS "_WIN64;_AMD64_;AMD64;_ATL_NO_WIN_SUPPORT")
else()
    message(FATAL_ERROR "Unknown architecture")
endif()

message(STATUS "WDK_ROOT: " ${WDK_ROOT})
message(STATUS "WDK_VERSION: " ${WDK_VERSION})
message(STATUS "WDK_COMPILE_DEFINITIONS: " ${WDK_COMPILE_DEFINITIONS})

# Windows 10 1709 by default
# https://learn.microsoft.com/ru-ru/cpp/porting/modifying-winver-and-win32-winnt
# https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
set(WDK_UMDF "2.27" CACHE STRING "Default version of UMDF for WDK targets")
set(WDK_WINVER "0x0A00" CACHE STRING "Default WINVER for WDK targets")
set(WDK_NTDDI_VERSION "0x0A000004" CACHE STRING "Specified NTDDI_VERSION for WDK targets if needed")

function(wdk_add_usermode_driver _target)
    cmake_parse_arguments(WDK "" "UMDF;WINVER;NTDDI_VERSION" "" ${ARGN})

    message(STATUS "WDK_UMDF: " ${WDK_UMDF})
    message(STATUS "WDK_WINVER: " ${WDK_WINVER})
    message(STATUS "WDK_NTDDI_VERSION: " ${WDK_NTDDI_VERSION})

    add_library(${_target} ${WDK_UNPARSED_ARGUMENTS})

    set_target_properties(${_target} PROPERTIES SUFFIX ".dll")
    set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "/Gz")
    set_target_properties(${_target} PROPERTIES COMPILE_DEFINITIONS
        "${WDK_COMPILE_DEFINITIONS};_WIN32_WINNT=${WDK_WINVER};NTDDI_VERSION=${WDK_NTDDI_VERSION}")
    set_target_properties(${_target} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

    set(WDK_WDF_DIR "${WDK_WDF_ROOT}/wdf/umdf/${WDK_UMDF}")
    set(WDK_WINRT_DIR "${WDK_ROOT}/winrt")
    set(WDK_UCRT_DIR "${WDK_ROOT}/ucrt")
    set(WDK_SHARED_DIR "${WDK_ROOT}/shared")
    set(WDK_UM_DIR "${WDK_ROOT}/um")
    set(WDK_IDDCX_DIR "${WDK_ROOT}/um/iddcx/${IDDCX_VERSION_MAJOR}.${IDDCX_VERSION_MINOR}")

    target_include_directories(${_target} SYSTEM PRIVATE
        "${WDK_WDF_DIR}"
        "${WDK_WINRT_DIR}"
        "${WDK_UCRT_DIR}"
        "${WDK_SHARED_DIR}"
        "${WDK_UM_DIR}"
        "${WDK_IDDCX_DIR}")

    message(STATUS "WDK_WDF_DIR: ${WDK_WDF_DIR}")
    message(STATUS "WDK_WINRT_DIR: ${WDK_WINRT_DIR}")
    message(STATUS "WDK_UCRT_DIR: ${WDK_UCRT_DIR}")
    message(STATUS "WDK_SHARED_DIR: ${WDK_SHARED_DIR}")
    message(STATUS "WDK_UM_DIR: ${WDK_UM_DIR}")
    message(STATUS "WDK_IDDCX_DIR: ${WDK_IDDCX_DIR}")

    target_link_libraries(${_target} OneCoreUAP avrt)
endfunction()
