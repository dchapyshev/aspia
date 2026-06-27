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
    router_manager.cc
    router_manager.h
    service.cc
    service.h
    service_main.cc
    service_main.h
    system_info_client.cc
    system_info_client.h
    system_settings.cc
    system_settings.h
    udp_attempt.cc
    udp_attempt.h
    user_session_agent.cc
    user_session_agent.h
    user_session.cc
    user_session.h)

if (WIN32)
    list(APPEND SOURCE_HOST_CORE
        desktop_resizer_win.cc
        desktop_resizer_win.h
        host_core.rc
        input_injector_win.cc
        input_injector_win.h)
endif()

if (LINUX)
    list(APPEND SOURCE_HOST_CORE
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
        input_injector_x11.h)
endif()

if (APPLE)
    list(APPEND SOURCE_HOST_CORE
        input_injector_mac.mm
        input_injector_mac.h)
endif()

include(ui/sources.cmake)

if (WIN32)
    include(win/sources.cmake)
endif()

source_group("" FILES ${SOURCE_HOST_CORE})
source_group(ui FILES ${SOURCE_HOST_CORE_UI})

if (WIN32)
    source_group(win FILES ${SOURCE_HOST_CORE_WIN})

    set(HOST_PLATFORM_LIBS
        avrt crypt32 dbghelp dxgi d3d11 imm32 iphlpapi netapi32 setupapi userenv uxtheme version
        winmm wtsapi32)
endif()

add_library(aspia_host_core SHARED
    ${SOURCE_HOST_CORE}
    ${SOURCE_HOST_CORE_UI}
    ${SOURCE_HOST_CORE_WIN})
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
    set(HOST_PLATFORM_LIBS
        stdc++fs
        ${XI_LIB}
        ${X11_LIB}
        ${XEXT_LIB})
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
