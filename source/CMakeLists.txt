#
# PROJECT:         Aspia
# FILE:            CMakeLists.txt
# LICENSE:         GNU General Public License 3
# PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
#

cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_SYSTEM_VERSION 7.0 CACHE TYPE INTERNAL FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION 8.1 CACHE TYPE INTERNAL FORCE)

set(ASPIA_THIRD_PARTY_DIR "$ENV{ASPIA_THIRD_PARTY_DIR}")

project(aspia)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "${ASPIA_THIRD_PARTY_DIR}/qt")
find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets WinExtras Xml)
find_package(Qt5LinguistTools)

include(aspia_core.cmake)

source_group(base FILES ${SOURCE_BASE})
source_group(base\\win FILES ${SOURCE_BASE_WIN})
source_group(client FILES ${SOURCE_CLIENT})
source_group(client\\ui FILES ${SOURCE_CLIENT_UI})
source_group(codec FILES ${SOURCE_CODEC})
source_group(console FILES ${SOURCE_CONSOLE})
source_group(crypto FILES ${SOURCE_CRYPTO})
source_group(desktop_capture FILES ${SOURCE_DESKTOP_CAPTURE})
source_group(desktop_capture\\win FILES ${SOURCE_DESKTOP_CAPTURE_WIN})
source_group(host FILES ${SOURCE_HOST})
source_group(host\\ui FILES ${SOURCE_HOST_UI})
source_group(host\\win FILES ${SOURCE_HOST_WIN})
source_group(ipc FILES ${SOURCE_IPC})
source_group(network FILES ${SOURCE_NETWORK})
source_group(protocol FILES ${SOURCE_PROTOCOL})
source_group(resources FILES ${SOURCE_RESOURCES})
source_group(system_info FILES ${SOURCE_SYSTEM_INFO})
source_group(system_info\\parser FILES ${SOURCE_SYSTEM_INFO_PARSER})
source_group(system_info\\protocol FILES ${SOURCE_SYSTEM_INFO_PROTOCOL})
source_group(system_info\\serializer FILES ${SOURCE_SYSTEM_INFO_SERIALIZER})
source_group(system_info\\ui FILES ${SOURCE_SYSTEM_INFO_UI})
source_group("" FILES ${SOURCE})

add_definitions(/FI"build_config.h")

add_definitions(-D_UNICODE
                -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
                -D_CRT_SECURE_NO_WARNINGS
                -DCORE_IMPLEMENTATION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /Oy /GL /MT /MP /arch:SSE2 /fp:fast /wd4146")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP /wd4146")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

add_library(aspia_core SHARED
    ${SOURCE_BASE}
    ${SOURCE_BASE_WIN}
    ${SOURCE_CLIENT}
    ${SOURCE_CLIENT_UI}
    ${SOURCE_CODEC}
    ${SOURCE_CONSOLE}
    ${SOURCE_CRYPTO}
    ${SOURCE_DESKTOP_CAPTURE}
    ${SOURCE_DESKTOP_CAPTURE_WIN}
    ${SOURCE_HOST}
    ${SOURCE_HOST_UI}
    ${SOURCE_HOST_WIN}
    ${SOURCE_IPC}
    ${SOURCE_NETWORK}
    ${SOURCE_PROTOCOL}
    ${SOURCE_RESOURCES}
    ${SOURCE_SYSTEM_INFO}
    ${SOURCE_SYSTEM_INFO_PARSER}
    ${SOURCE_SYSTEM_INFO_PROTOCOL}
    ${SOURCE_SYSTEM_INFO_SERIALIZER}
    ${SOURCE_SYSTEM_INFO_UI}
    ${SOURCE})

target_link_libraries(aspia_core
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
    debug libsodiumd
    debug libvpxd
    debug libyuvd
    debug qtfreetyped
    debug qtharfbuzzd
    debug qtlibpngd
    debug qtpcre2d
    debug qwindowsd
    debug qwindowsvistastyled
    debug zlib-ngd
    optimized Qt5AccessibilitySupport
    optimized Qt5EventDispatcherSupport
    optimized Qt5FontDatabaseSupport
    optimized Qt5ThemeSupport
    optimized Qt5WindowsUIAutomationSupport
    optimized libprotobuf-lite
    optimized libsodium
    optimized libvpx
    optimized libyuv
    optimized qtfreetype
    optimized qtharfbuzz
    optimized qtlibpng
    optimized qtpcre2
    optimized qwindows
    optimized qwindowsvistastyle
    optimized zlib-ng
    crypt32
    dwmapi
    imm32
    iphlpapi
    mpr
    netapi32
    sas
    userenv
    uxtheme
    version
    winmm
    ws2_32
    wtsapi32)

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

add_executable(aspia_host_notifier ${PROJECT_SOURCE_DIR}/host/host_notifier_entry_point.cc ${PROJECT_SOURCE_DIR}/host/host_notifier.rc)
set_target_properties(aspia_host_notifier PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(aspia_host_notifier aspia_core)

add_subdirectory(translations)
