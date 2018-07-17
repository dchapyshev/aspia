#
# PROJECT:         Aspia
# FILE:            translations/CMakeLists.txt
# LICENSE:         GNU General Public License 3
# PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
#

if(Qt5LinguistTools_FOUND)
    # Don't remove TS files on make clean.
    set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM TRUE)

    # List of translation files.
    set(TS_FILES aspia_de.ts aspia_ru.ts aspia_nl.ts)

    # Get the list of files containing messages.
    file(GLOB_RECURSE TR_SOURCES
        ${PROJECT_SOURCE_DIR}/*.cc
        ${PROJECT_SOURCE_DIR}/*.cpp
        ${PROJECT_SOURCE_DIR}/*.h
        ${PROJECT_SOURCE_DIR}/*.ui)

    qt5_add_translation(QM_UPDATE ${TS_FILES})
    add_custom_target(aspia_qm_update ALL DEPENDS ${QM_UPDATE})

    qt5_create_translation(TS_UPDATE ${TR_SOURCES} ${TS_FILES} OPTIONS "-no-obsolete")
    add_custom_target(aspia_ts_update DEPENDS ${TS_UPDATE})

    foreach(QM_FILE ${QM_UPDATE})
        get_filename_component(QM_FILE_NAME ${QM_FILE} NAME)
        add_custom_command(TARGET aspia_qm_update POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QM_FILE}" "${PROJECT_BINARY_DIR}/$<CONFIG>/translations/${QM_FILE_NAME}")
    endforeach()
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
