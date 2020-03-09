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

include(tbb)
include(translations)

list(APPEND SOURCE_ROUTER_MANAGER
    main.cc
    main_window.cc
    main_window.h
    main_window.ui
    router_manager.rc)

list(APPEND SOURCE_ROUTER_MANAGER_RESOURCES
    resources/router_manager.qrc)

source_group("" FILES ${SOURCE_ROUTER_MANAGER})
source_group(moc FILES ${SOURCE_ROUTER_MANAGER_MOC})
source_group(resources FILES ${SOURCE_ROUTER_MANAGER_RESOURCES})

add_executable(aspia_router_manager
    ${SOURCE_ROUTER_MANAGER}
    ${SOURCE_ROUTER_MANAGER_RESOURCES})
set_target_properties(aspia_router_manager PROPERTIES WIN32_EXECUTABLE TRUE)
set_target_properties(aspia_router_manager PROPERTIES LINK_FLAGS "/MANIFEST:NO")
target_link_libraries(aspia_router_manager
    aspia_base
    aspia_common
    aspia_crypto
    aspia_net
    aspia_proto
    ${QT_LIBS}
    ${WINDOWS_LIBS}
    ${THIRD_PARTY_LIBS})

if (USE_TBB)
    add_tbb(aspia_router_manager ${ASPIA_THIRD_PARTY_DIR}/tbb)
endif()

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB ROUTER_MANAGER_TS_FILES translations/*.ts)
    add_translations(aspia_router_manager QRC_FILE "translations/router_manager_translations.qrc" TS_FILES ${ROUTER_MANAGER_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE ROUTER_MANAGER_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/router/ui/*.cc
        ${PROJECT_SOURCE_DIR}/router/ui/*.cpp
        ${PROJECT_SOURCE_DIR}/router/ui/*.h
        ${PROJECT_SOURCE_DIR}/router/ui/*.ui)

    qt5_create_translation(TS_ROUTER_MANAGER_UPDATE ${ROUTER_MANAGER_TR_SOURCES} ${ROUTER_MANAGER_TS_FILES})
    add_custom_target(aspia_router_manager_ts_update DEPENDS ${TS_ROUTER_MANAGER_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
