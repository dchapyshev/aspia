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

include(translations)

list(APPEND SOURCE_ROUTER_MANAGER
    main.cc
    main_window.cc
    main_window.h
    main_window.ui
    mru_cache.cc
    mru_cache.h
    router.cc
    router.h
    router_dialog.cc
    router_dialog.h
    router_dialog.ui
    router_manager.rc
    router_proxy.cc
    router_proxy.h
    router_window.h
    router_window_proxy.cc
    router_window_proxy.h
    settings.cc
    settings.h
    status_dialog.cc
    status_dialog.h
    status_dialog.ui
    user_dialog.cc
    user_dialog.h
    user_dialog.ui)

list(APPEND SOURCE_ROUTER_MANAGER_RESOURCES resources/router_manager.qrc)

source_group("" FILES ${SOURCE_ROUTER_MANAGER})
source_group(moc FILES ${SOURCE_ROUTER_MANAGER_MOC})
source_group(resources FILES ${SOURCE_ROUTER_MANAGER_RESOURCES})

if (WIN32)
    set(ROUTER_MANAGER_PLATFORM_LIBS
        crypt32
        dwmapi
        imm32
        netapi32
        userenv
        uxtheme
        version
        winmm
        wtsapi32)
endif()

if (LINUX)
    set(ROUTER_MANAGER_PLATFORM_LIBS
        stdc++fs
        optimized ${ICUUC_RELEASE}
        optimized ${ICUDATA_RELEASE}
        debug ${ICUUC_DEBUG}
        debug ${ICUDATA_DEBUG})
endif()

if (APPLE)
    set(ROUTER_MANAGER_PLATFORM_LIBS
        optimized ${ICUUC_RELEASE}
        optimized ${ICUDATA_RELEASE}
        debug ${ICUUC_DEBUG}
        debug ${ICUDATA_DEBUG})
endif()

add_executable(aspia_router_manager
    ${SOURCE_ROUTER_MANAGER}
    ${SOURCE_ROUTER_MANAGER_RESOURCES})
set_target_properties(aspia_router_manager PROPERTIES WIN32_EXECUTABLE TRUE)

if (WIN32)
    set_target_properties(aspia_router_manager PROPERTIES LINK_FLAGS "/MANIFEST:NO")
endif()

target_link_libraries(aspia_router_manager
    aspia_base
    aspia_common
    aspia_proto
    aspia_qt_base
    ${ROUTER_MANAGER_PLATFORM_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS})
set_property(TARGET aspia_router_manager PROPERTY AUTOMOC ON)
set_property(TARGET aspia_router_manager PROPERTY AUTOUIC ON)
set_property(TARGET aspia_router_manager PROPERTY AUTORCC ON)
qt5_import_plugins(aspia_router_manager
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB ROUTER_MANAGER_TS_FILES translations/*.ts)
    add_translations(aspia_router_manager QRC_FILE "translations/router_manager_translations.qrc" TS_FILES ${ROUTER_MANAGER_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE ROUTER_MANAGER_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/router/manager/*.cc
        ${PROJECT_SOURCE_DIR}/router/manager/*.cpp
        ${PROJECT_SOURCE_DIR}/router/manager/*.h
        ${PROJECT_SOURCE_DIR}/router/manager/*.ui)

    qt5_create_translation(TS_ROUTER_MANAGER_UPDATE ${ROUTER_MANAGER_TR_SOURCES} ${ROUTER_MANAGER_TS_FILES})
    add_custom_target(aspia_router_manager_ts_update DEPENDS ${TS_ROUTER_MANAGER_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
