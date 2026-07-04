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

list(APPEND SOURCE_HOST_CORE
    capture_scheduler.cc
    capture_scheduler.h
    chat_client.cc
    chat_client.h
    client.cc
    client.h
    database.cc
    database.h
    desktop_agent.cc
    desktop_agent.h
    desktop_agent_client.cc
    desktop_agent_client.h
    desktop_agent_main.cc
    desktop_agent_main.h
    desktop_client.cc
    desktop_client.h
    desktop_environment.cc
    desktop_environment.h
    desktop_manager.cc
    desktop_manager.h
    desktop_resizer.cc
    desktop_resizer.h
    file_agent.cc
    file_agent.h
    file_agent_main.cc
    file_agent_main.h
    file_client.cc
    file_client.h
    host_export.h
    host_storage.cc
    host_storage.h
    host_user_list.cc
    host_user_list.h
    host_utils.cc
    host_utils.h
    input_injector.h
    process_monitor.cc
    process_monitor.h
    router_manager.cc
    router_manager.h
    screen_capturer.cc
    screen_capturer.h
    screen_capturer_helper.cc
    screen_capturer_helper.h
    service.cc
    service.h
    service_main.cc
    service_main.h
    system_info.cc
    system_info.h
    system_info_client.cc
    system_info_client.h
    system_settings.cc
    system_settings.h
    task_manager.cc
    task_manager.h
    terminal_agent.cc
    terminal_agent.h
    terminal_agent_main.cc
    terminal_agent_main.h
    terminal_client.cc
    terminal_client.h
    terminal_process.cc
    terminal_process.h
    udp_attempt.cc
    udp_attempt.h
    user_session_agent.cc
    user_session_agent.h
    user_session.cc
    user_session.h)

if (WIN32)
    list(APPEND SOURCE_HOST_CORE
        desktop_environment_win.cc
        desktop_environment_win.h
        desktop_resizer_win.cc
        desktop_resizer_win.h
        host_core.rc
        input_injector_win.cc
        input_injector_win.h
        process_monitor_win.cc
        process_monitor_win.h
        screen_capturer_dxgi.cc
        screen_capturer_dxgi.h
        screen_capturer_gdi.cc
        screen_capturer_gdi.h
        screen_capturer_win.cc
        screen_capturer_win.h
        terminal_process_win.cc
        terminal_process_win.h)
endif()

if (LINUX)
    list(APPEND SOURCE_HOST_CORE
        desktop_environment_linux.cc
        desktop_environment_linux.h
        desktop_resizer_vt.cc
        desktop_resizer_vt.h
        desktop_resizer_x11.cc
        desktop_resizer_x11.h
        input_injector_uinput.cc
        input_injector_uinput.h
        input_injector_vt.cc
        input_injector_vt.h
        input_injector_wayland.cc
        input_injector_wayland.h
        input_injector_x11.cc
        input_injector_x11.h
        process_monitor_linux.cc
        process_monitor_linux.h
        screen_capturer_kms.cc
        screen_capturer_kms.h
        screen_capturer_kwin.cc
        screen_capturer_kwin.h
        screen_capturer_pipewire.cc
        screen_capturer_pipewire.h
        screen_capturer_vt.cc
        screen_capturer_vt.h
        screen_capturer_wlr.cc
        screen_capturer_wlr.h
        screen_capturer_x11.cc
        screen_capturer_x11.h
        terminal_process_unix.cc
        terminal_process_unix.h)
endif()

if (APPLE)
    list(APPEND SOURCE_HOST_CORE
        desktop_environment_mac.mm
        desktop_environment_mac.h
        input_injector_mac.mm
        input_injector_mac.h
        process_monitor_mac.mm
        process_monitor_mac.h
        screen_capturer_mac.mm
        screen_capturer_mac.h
        terminal_process_unix.cc
        terminal_process_unix.h)
endif()

include(ui/sources.cmake)

if (WIN32)
    include(win/sources.cmake)
endif()

if (LINUX)
    include(linux/sources.cmake)
endif()

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(ui FILES ${SOURCE_HOST_CORE_UI})

if (LINUX)
    source_group(linux FILES ${SOURCE_HOST_CORE_LINUX})
endif()

if (WIN32)
    source_group(win FILES ${SOURCE_HOST_CORE_WIN})

    set(HOST_PLATFORM_LIBS
        advapi32 avrt crypt32 dbghelp dxgi d3d11 imm32 iphlpapi netapi32 setupapi userenv uxtheme
        version winmm wtsapi32)
endif()

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
    ${SOURCE_HOST_CORE_UI}
    ${SOURCE_HOST_CORE_WIN}
    ${SOURCE_HOST_CORE_LINUX})
target_link_libraries(aspia_host_core PRIVATE
    aspia_base
    aspia_common
    aspia_proto
    ${HOST_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_target_properties(aspia_host_core PROPERTIES COMPILE_DEFINITIONS "HOST_IMPLEMENTATION")
set_property(TARGET aspia_host_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_host_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_host_core PROPERTY AUTORCC ON)
qt_import_plugins(aspia_host_core INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE imageformats iconengines EXCLUDE_BY_TYPE "")

if (LINUX)
    # The screen capturers compile the PipeWire/DRM stubs (host/linux); only their headers are needed
    # at build time (PipeWire/EGL/GBM/libdrm are loaded dynamically). The wlr-screencopy client stubs
    # come from the third_party/wlr-protocols static library.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
    pkg_check_modules(LIBDRM REQUIRED libdrm)
    # Wayland client comes from the static vcpkg port (libwayland-client.a pulls in libffi); link both
    # explicitly so nothing links the system libwayland-client. List the static archive name first: the
    # build host also ships a system libwayland-client.so that find_library would otherwise prefer. The
    # wlr-screencopy protocol stubs are the third_party/wlr-protocols static library.
    find_library(WAYLAND_CLIENT_LIB NAMES libwayland-client.a wayland-client REQUIRED)
    find_library(FFI_LIB NAMES ffi REQUIRED)
    target_include_directories(aspia_host_core PRIVATE
        ${PIPEWIRE_INCLUDE_DIRS} ${LIBDRM_INCLUDE_DIRS})
    target_link_libraries(aspia_host_core PRIVATE ${WAYLAND_CLIENT_LIB} ${FFI_LIB} wlr_protocols)
endif()

if (LINUX OR APPLE)
    # PAM is used to validate operating system credentials for terminal sessions.
    find_library(PAM_LIB NAMES pam REQUIRED)
    target_link_libraries(aspia_host_core PRIVATE ${PAM_LIB})
endif()

if (APPLE)
    target_link_libraries(aspia_host_core PRIVATE
        ${SCREENCAPTUREKIT_LIB}
        ${COREMEDIA_LIB}
        ${COREVIDEO_LIB}
        ${COREGRAPHICS_LIB})
endif()

if (LINUX)
    # forkpty() for terminal sessions lives in libutil.
    target_link_libraries(aspia_host_core PRIVATE util)
endif()

if (LINUX)
    set(HOST_PLATFORM_LIBS stdc++fs)
endif()

if (WIN32)
    set(HOST_PLATFORM_LIBS Qt6::EntryPointPrivate Qt6::EntryPointImplementation)
endif()

# Aspia Host (GUI)
add_executable(aspia_host ui/host_entry_point.cc ui/host.rc)
if (WIN32)
    set_target_properties(aspia_host PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_host PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_host aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia Host Service
add_executable(aspia_host_service service_entry_point.cc service.rc)
if (WIN32)
    set_target_properties(aspia_host_service PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_host_service aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia Desktop Agent
add_executable(aspia_desktop_agent WIN32 desktop_agent_entry_point.cc desktop_agent.rc)
if (WIN32)
    set_target_properties(aspia_desktop_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_desktop_agent aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia File Transfer Agent
add_executable(aspia_file_agent WIN32 file_agent_entry_point.cc file_agent.rc)
if (WIN32)
    set_target_properties(aspia_file_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_file_agent aspia_host_core ${HOST_PLATFORM_LIBS})

# Aspia Terminal Agent
add_executable(aspia_terminal_agent WIN32 terminal_agent_entry_point.cc terminal_agent.rc)
if (WIN32)
    set_target_properties(aspia_terminal_agent PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()
target_link_libraries(aspia_terminal_agent aspia_host_core ${HOST_PLATFORM_LIBS})
