#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

if(Qt5LinguistTools_FOUND)
    # Don't remove TS files on make clean.
    set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM TRUE)

    # List of translation files.
    set(TS_UPDATER_FILES aspia_updater_de.ts aspia_updater_ru.ts aspia_updater_nl.ts aspia_updater_uk.ts)

    # Get the list of files containing messages.
    file(GLOB_RECURSE TR_UPDATER_SOURCES
        ${PROJECT_SOURCE_DIR}/updater/*.cc
        ${PROJECT_SOURCE_DIR}/updater/*.cpp
        ${PROJECT_SOURCE_DIR}/updater/*.h
        ${PROJECT_SOURCE_DIR}/updater/*.ui)

    qt5_add_translation(QM_UPDATER_UPDATE ${TS_UPDATER_FILES})
    add_custom_target(aspia_updater_qm_update ALL DEPENDS ${QM_UPDATER_UPDATE})

    qt5_create_translation(TS_UPDATER_UPDATE ${TR_UPDATER_SOURCES} ${TS_UPDATER_FILES} OPTIONS "-no-obsolete")
    add_custom_target(aspia_updater_ts_update DEPENDS ${TS_UPDATER_UPDATE})

    foreach(QM_FILE ${QM_UPDATER_UPDATE})
        get_filename_component(QM_FILE_NAME ${QM_FILE} NAME)
        add_custom_command(TARGET aspia_updater_qm_update POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QM_FILE}" "${PROJECT_BINARY_DIR}/$<CONFIG>/translations/${QM_FILE_NAME}")
    endforeach()
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
