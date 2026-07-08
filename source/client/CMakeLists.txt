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

list(APPEND SOURCE_CLIENT_CORE
    aab_importer.cc
    aab_importer.h
    application.cc
    application.h
    database.cc
    database.h
    client.cc
    client.h
    config.cc
    config.h
    client_desktop.cc
    client_desktop.h
    client_file_transfer.cc
    client_file_transfer.h
    main.cc
    client_system_info.cc
    client_system_info.h
    client_terminal.cc
    client_terminal.h
    client_text_chat.cc
    client_text_chat.h
    file_error_code.cc
    file_error_code.h
    file_remove_queue_builder.cc
    file_remove_queue_builder.h
    file_remover.cc
    file_remover.h
    file_transfer.cc
    file_transfer.h
    file_transfer_queue_builder.cc
    file_transfer_queue_builder.h
    json_backup.cc
    json_backup.h
    master_password.cc
    master_password.h
    router.cc
    router.h
    session_keeper.cc
    session_keeper.h
    session_state.cc
    session_state.h
    settings.cc
    settings.h
    udp_attempt.cc
    udp_attempt.h)

if (WIN32)
    list(APPEND SOURCE_CLIENT_CORE client.rc)
endif()

include(android/sources.cmake)
include(online_checker/sources.cmake)
include(desktop/sources.cmake)
include(desktop/desktop/sources.cmake)
include(desktop/file_transfer/sources.cmake)
include(desktop/sys_info/sources.cmake)
include(desktop/chat/sources.cmake)
include(desktop/hosts/sources.cmake)
include(desktop/terminal/sources.cmake)

source_group("" FILES ${SOURCE_CLIENT_CORE})
source_group(android FILES ${SOURCE_CLIENT_ANDROID})
source_group(online_checker FILES ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
source_group(desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP})
source_group(desktop\\desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP})
source_group(desktop\\file_transfer FILES ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER})
source_group(desktop\\sys_info FILES ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO})
source_group(desktop\\chat FILES ${SOURCE_CLIENT_CORE_DESKTOP_CHAT})
source_group(desktop\\terminal FILES ${SOURCE_CLIENT_CORE_DESKTOP_TERMINAL})
source_group(desktop\\hosts FILES ${SOURCE_CLIENT_CORE_DESKTOP_HOSTS_TAB})

if (WIN32)
    set(CLIENT_PLATFORM_LIBS
        avrt crypt32 dbghelp dwmapi imm32 iphlpapi netapi32 shlwapi userenv uxtheme version
        winmm wtsapi32)
endif()

if (LINUX)
    set(CLIENT_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    # VideoToolbox/CoreMedia/CoreVideo back the hardware H264 decoder.
    set(CLIENT_PLATFORM_LIBS
        ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} portaudio
        ${VIDEOTOOLBOX_LIB} ${COREMEDIA_LIB} ${COREVIDEO_LIB})
    set(aspia_client_ICON ${PROJECT_SOURCE_DIR}/source/common/resources/aspia.icns)
    set(aspia_client_PLIST ${PROJECT_SOURCE_DIR}/source/client/Info.plist)

    set_source_files_properties(${aspia_client_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if (ANDROID)
    qt_add_executable(aspia_client
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_ANDROID}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
else()
    add_executable(aspia_client MACOSX_BUNDLE
        ${aspia_client_ICON}
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER}
        ${SOURCE_CLIENT_CORE_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER}
        ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO}
        ${SOURCE_CLIENT_CORE_DESKTOP_CHAT}
        ${SOURCE_CLIENT_CORE_DESKTOP_TERMINAL}
        ${SOURCE_CLIENT_CORE_DESKTOP_HOSTS_TAB})
endif()

target_link_libraries(aspia_client PRIVATE
    aspia_base
    aspia_common
    aspia_proto
    ${CLIENT_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_client PROPERTY AUTOMOC ON)
set_property(TARGET aspia_client PROPERTY AUTOUIC ON)
set_property(TARGET aspia_client PROPERTY AUTORCC ON)
qt_import_plugins(aspia_client INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE imageformats iconengines EXCLUDE_BY_TYPE "")

if (ANDROID)
    set_target_properties(aspia_client PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.aspia.client"
        QT_ANDROID_APP_NAME "Aspia Client"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/packaging")
endif()

if (WIN32)
    set_target_properties(aspia_client PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(aspia_client PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

if (APPLE)
    set_target_properties(aspia_client PROPERTIES OUTPUT_NAME "Aspia Client")
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${aspia_client_PLIST})

    # Localized bundle display name.
    foreach(client_lproj fr ru uk zh-Hans)
        set(CLIENT_LPROJ_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lproj/${client_lproj}.lproj/InfoPlist.strings)
        set_source_files_properties(${CLIENT_LPROJ_FILE} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources/${client_lproj}.lproj)
        target_sources(aspia_client PRIVATE ${CLIENT_LPROJ_FILE})
    endforeach()
endif()
