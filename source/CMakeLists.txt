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
find_package(Qt5 REQUIRED Core Gui Network PrintSupport Widgets WinExtras)

include(aspia_core.cmake)

source_group(base FILES ${SOURCE_BASE})
source_group(base\\message_loop FILES ${SOURCE_BASE_MESSAGE_LOOP})
source_group(base\\process FILES ${SOURCE_BASE_PROCESS})
source_group(base\\strings FILES ${SOURCE_BASE_STRINGS})
source_group(base\\threading FILES ${SOURCE_BASE_THREADING})
source_group(base\\win FILES ${SOURCE_BASE_WIN})
source_group(client FILES ${SOURCE_CLIENT})
source_group(client\\ui FILES ${SOURCE_CLIENT_UI})
source_group(codec FILES ${SOURCE_CODEC})
source_group(console FILES ${SOURCE_CONSOLE})
source_group(crypto FILES ${SOURCE_CRYPTO})
source_group(desktop_capture FILES ${SOURCE_DESKTOP_CAPTURE})
source_group(host FILES ${SOURCE_HOST})
source_group(host\\ui FILES ${SOURCE_HOST_UI})
source_group(ipc FILES ${SOURCE_IPC})
source_group(network FILES ${SOURCE_NETWORK})
source_group(protocol FILES ${SOURCE_PROTOCOL})
source_group(resources FILES ${SOURCE_RESOURCES})
source_group("" FILES ${SOURCE})

add_definitions(/FI"build_config.h")

add_definitions(-D_UNICODE
                -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
                -D_CRT_SECURE_NO_WARNINGS
                -DCORE_IMPLEMENTATION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /GL /MT /MP /wd4146")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /MP /wd4146")
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /i${THIRD_PARTY_DIR}/wtl/include")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF")

add_library(aspia_core SHARED
            ${SOURCE_BASE}
            ${SOURCE_BASE_MESSAGE_LOOP}
            ${SOURCE_BASE_PROCESS}
            ${SOURCE_BASE_STRINGS}
            ${SOURCE_BASE_THREADING}
            ${SOURCE_BASE_WIN}
            ${SOURCE_CLIENT}
            ${SOURCE_CLIENT_UI}
            ${SOURCE_CODEC}
            ${SOURCE_CONSOLE}
            ${SOURCE_CRYPTO}
            ${SOURCE_DESKTOP_CAPTURE}
            ${SOURCE_HOST}
            ${SOURCE_HOST_UI}
            ${SOURCE_IPC}
            ${SOURCE_NETWORK}
            ${SOURCE_PROTOCOL}
            ${SOURCE_RESOURCES}
            ${SOURCE})

target_link_libraries(aspia_core
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::PrintSupport
    Qt5::Widgets
    Qt5::WinMain
    Qt5::WinExtras
    debug Qt5AccessibilitySupportd
    debug Qt5EventDispatcherSupportd
    debug Qt5FontDatabaseSupportd
    debug Qt5ThemeSupportd
    debug libprotobuf-lited
    debug libsodiumd
    debug libvpxd
    debug libyuvd
    debug qtfreetyped
    debug qtharfbuzzd
    debug qtlibpngd
    debug qtpcre2d
    debug qwindowsd
    debug zlib-ngd
    optimized Qt5AccessibilitySupport
    optimized Qt5EventDispatcherSupport
    optimized Qt5FontDatabaseSupport
    optimized Qt5ThemeSupport
    optimized libprotobuf-lite
    optimized libsodium
    optimized libvpx
    optimized libyuv
    optimized qtfreetype
    optimized qtharfbuzz
    optimized qtlibpng
    optimized qtpcre2
    optimized qwindows
    optimized zlib-ng
    comctl32
    crypt32
    dbghelp
    dwmapi
    imm32
    iphlpapi
    mpr
    msi
    netapi32
    psapi
    rasapi32
    sas
    secur32
    setupapi
    shlwapi
    taskschd
    userenv
    uxtheme
    version
    winmm
    ws2_32
    wtsapi32)

add_executable(aspia_console ${PROJECT_SOURCE_DIR}/console/entry_point.cc ${PROJECT_SOURCE_DIR}/console/console.rc)
set_target_properties(aspia_console PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(aspia_console aspia_core)

add_executable(aspia_client ${PROJECT_SOURCE_DIR}/client/entry_point.cc ${PROJECT_SOURCE_DIR}/client/client.rc)
set_target_properties(aspia_client PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(aspia_client aspia_core)

add_executable(aspia_host_config ${PROJECT_SOURCE_DIR}/host/host_config_entry_point.cc ${PROJECT_SOURCE_DIR}/host/host_config.rc)
set_target_properties(aspia_host_config PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host_config PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host_config aspia_core)

add_executable(aspia_host_service ${PROJECT_SOURCE_DIR}/host/host_service_entry_point.cc ${PROJECT_SOURCE_DIR}/host/host_service.rc)
target_link_libraries(aspia_host_service aspia_core)

add_executable(aspia_host ${PROJECT_SOURCE_DIR}/host/host_entry_point.cc ${PROJECT_SOURCE_DIR}/host/host.rc)
set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_host aspia_core)
