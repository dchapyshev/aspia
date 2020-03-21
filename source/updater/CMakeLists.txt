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

list(APPEND SOURCE_UPDATER
    download_dialog.cc
    download_dialog.h
    download_dialog.ui
    update_checker.cc
    update_checker.h
    update_checker_impl.cc
    update_checker_impl.h
    update_dialog.cc
    update_dialog.h
    update_dialog.ui
    update_info.cc
    update_info.h)

list(APPEND SOURCE_UPDATER_RESOURCES
    resources/updater.qrc)

source_group("" FILES ${SOURCE_UPDATER})
source_group(resources FILES ${SOURCE_UPDATER_RESOURCES})
source_group(ui FILES ${SOURCE_UPDATER_UI})

add_library(aspia_updater STATIC
    ${SOURCE_UPDATER}
    ${SOURCE_UPDATER_RESOURCES}
    ${SOURCE_UPDATER_UI})
target_link_libraries(aspia_updater aspia_base ${QT_LIBS} ${THIRD_PARTY_LIBS})

if(Qt5LinguistTools_FOUND)
    # Get the list of translation files.
    file(GLOB UPDATER_TS_FILES translations/*.ts)

    add_translations(aspia_updater QRC_FILE "translations/updater_translations.qrc" TS_FILES ${UPDATER_TS_FILES})

    # Get the list of files containing messages.
    file(GLOB_RECURSE UPDATER_TR_SOURCES
        ${PROJECT_SOURCE_DIR}/updater/*.cc
        ${PROJECT_SOURCE_DIR}/updater/*.cpp
        ${PROJECT_SOURCE_DIR}/updater/*.h
        ${PROJECT_SOURCE_DIR}/updater/*.ui)

    create_translation(TS_UPDATER_UPDATE ${UPDATER_TR_SOURCES} ${UPDATER_TS_FILES})
    add_custom_target(aspia_updater_ts_update DEPENDS ${TS_UPDATER_UPDATE})
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
