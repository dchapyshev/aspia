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

# The implementation lives in a static library so both the client binary and the unit tests
# link the same objects; the executable itself is only the entry point.
list(APPEND SOURCE_CLIENT_CORE
    aab_importer.cc
    aab_importer.h
    application.cc
    application.h
    audio_output.cc
    audio_output.h
    audio_player.cc
    audio_player.h
    database.cc
    database.h
    config.cc
    config.h
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
    host_url.cc
    host_url.h
    json_backup.cc
    json_backup.h
    master_password.cc
    master_password.h
    router.cc
    router.h
    router_rpc.h
    router_state.cc
    router_state.h
    page_model.cc
    page_model.h
    search_page_model.cc
    search_page_model.h
    router_types.h
    session_keeper.cc
    session_keeper.h
    session_state.cc
    session_state.h
    settings.cc
    settings.h
    udp_attempt.cc
    udp_attempt.h)

list(APPEND SOURCE_CLIENT
    main.cc)

list(APPEND SOURCE_CLIENT_TESTS
    database_unittest.cc
    desktop/management/client_list_model_unittest.cc
    desktop/management/edit_model_unittest.cc
    desktop/management/host_list_model_unittest.cc
    desktop/management/local_host_list_model_unittest.cc
    desktop/management/relay_list_model_unittest.cc
    desktop/management/search_result_model_unittest.cc
    desktop/management/temp_host_list_model_unittest.cc
    desktop/management/user_list_model_unittest.cc
    json_backup_unittest.cc
    router_rpc_unittest.cc
    router_state_unittest.cc
    page_model_unittest.cc
    search_page_model_unittest.cc
    tests_main.cc)

if (WIN32)
    list(APPEND SOURCE_CLIENT_CORE
        audio_output_win.cc
        audio_output_win.h)
    list(APPEND SOURCE_CLIENT client.rc)
endif()

if (LINUX)
    list(APPEND SOURCE_CLIENT_CORE
        audio_output_pulse.cc
        audio_output_pulse.h)
endif()

if (APPLE)
    list(APPEND SOURCE_CLIENT_CORE
        audio_output_mac.cc
        audio_output_mac.h
        session_keeper_mac.cc
        session_keeper_mac.h)
endif()

if (ANDROID)
    list(APPEND SOURCE_CLIENT_CORE
        audio_output_android.cc
        audio_output_android.h)
endif()

include(android/sources.cmake)
include(online_checker/sources.cmake)
include(desktop/sources.cmake)
include(desktop/desktop/sources.cmake)
include(desktop/file_transfer/sources.cmake)
include(desktop/sys_info/sources.cmake)
include(desktop/chat/sources.cmake)
include(desktop/management/sources.cmake)
include(desktop/terminal/sources.cmake)
include(workers/sources.cmake)

if (WIN32)
    include(win/sources.cmake)
endif()

source_group("" FILES ${SOURCE_CLIENT_CORE} ${SOURCE_CLIENT})
source_group(tests FILES ${SOURCE_CLIENT_TESTS})
source_group(android FILES ${SOURCE_CLIENT_ANDROID})
source_group(online_checker FILES ${SOURCE_CLIENT_CORE_ONLINE_CHECKER})
source_group(desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP})
source_group(desktop\\desktop FILES ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP})
source_group(desktop\\file_transfer FILES ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER})
source_group(desktop\\sys_info FILES ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO})
source_group(desktop\\chat FILES ${SOURCE_CLIENT_CORE_DESKTOP_CHAT})
source_group(desktop\\terminal FILES ${SOURCE_CLIENT_CORE_DESKTOP_TERMINAL})
source_group(desktop\\management FILES ${SOURCE_CLIENT_CORE_DESKTOP_MANAGEMENT_TAB})
source_group(win FILES ${SOURCE_CLIENT_WIN})
source_group(workers FILES ${SOURCE_CLIENT_WORKERS})

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
    configure_file(${PROJECT_SOURCE_DIR}/source/client/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)
    set(aspia_client_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)

    set_source_files_properties(${aspia_client_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if (ANDROID)
    add_library(aspia_client_core STATIC
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_ANDROID}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER}
        ${SOURCE_CLIENT_WORKERS})
else()
    add_library(aspia_client_core STATIC
        ${SOURCE_CLIENT_CORE}
        ${SOURCE_CLIENT_CORE_ONLINE_CHECKER}
        ${SOURCE_CLIENT_CORE_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_DESKTOP}
        ${SOURCE_CLIENT_CORE_DESKTOP_FILE_TRANSFER}
        ${SOURCE_CLIENT_CORE_DESKTOP_SYS_INFO}
        ${SOURCE_CLIENT_CORE_DESKTOP_CHAT}
        ${SOURCE_CLIENT_CORE_DESKTOP_TERMINAL}
        ${SOURCE_CLIENT_CORE_DESKTOP_MANAGEMENT_TAB}
        ${SOURCE_CLIENT_WIN}
        ${SOURCE_CLIENT_WORKERS})
endif()

target_link_libraries(aspia_client_core
    aspia_base
    aspia_common
    aspia_proto
    ${CLIENT_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})

set_property(TARGET aspia_client_core PROPERTY AUTOMOC ON)
set_property(TARGET aspia_client_core PROPERTY AUTOUIC ON)
set_property(TARGET aspia_client_core PROPERTY AUTORCC ON)

if (ANDROID)
    qt_add_executable(aspia_client ${SOURCE_CLIENT})
else()
    add_executable(aspia_client MACOSX_BUNDLE
        ${aspia_client_ICON}
        ${SOURCE_CLIENT})
endif()

target_link_libraries(aspia_client PRIVATE
    aspia_client_core)
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

if (NOT ANDROID)
    add_executable(aspia_client_tests ${SOURCE_CLIENT_TESTS})

    target_link_libraries(aspia_client_tests
        aspia_client_core
        GTest::gtest
        Qt6::Test
        ${THIRD_PARTY_LIBS})

    set_property(TARGET aspia_client_tests PROPERTY AUTOMOC ON)
    set_property(TARGET aspia_client_tests PROPERTY AUTOUIC ON)
    set_property(TARGET aspia_client_tests PROPERTY AUTORCC ON)
    qt_import_plugins(aspia_client_tests INCLUDE "" EXCLUDE "" INCLUDE_BY_TYPE "" EXCLUDE_BY_TYPE imageformats)

    add_test(NAME aspia_client_tests COMMAND aspia_client_tests)
endif()

if (APPLE)
    set_target_properties(aspia_client PROPERTIES OUTPUT_NAME "Aspia Client")
    set_target_properties(aspia_client PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${aspia_client_PLIST})

    # Localized bundle display name.
    foreach(client_lproj cs da de el es fa fi fr he hu it ja ko nb nl pl pt-BR pt-PT ru sv tr uk zh-Hans zh-Hant)
        set(CLIENT_LPROJ_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lproj/${client_lproj}.lproj/InfoPlist.strings)
        set_source_files_properties(${CLIENT_LPROJ_FILE} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources/${client_lproj}.lproj)
        target_sources(aspia_client PRIVATE ${CLIENT_LPROJ_FILE})
    endforeach()
endif()
