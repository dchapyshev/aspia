#
# Aspia Project
# Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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
# File based on qBittorrent source code.
# URL: https://github.com/qbittorrent/qBittorrent/blob/master/cmake/Modules/QbtTranslations.cmake
#

# add_translations(<target> QRC_FILE <filename> TS_FILES <filenames>)
# Handles out of source builds for Qt resource files that include translations.
# The function generates translations out of the supplied list of .ts files in the build directory,
# copies the .qrc file there, calls qt5_add_resources() adds its output to the target sources list.
function(add_translations _target)
    set(oneValueArgs QRC_FILE)
    set(multiValueArgs TS_FILES)
    cmake_parse_arguments(ASPIA_TR "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_target_property(_binaryDir ${_target} BINARY_DIR)

    if (NOT ASPIA_TR_QRC_FILE)
        message(FATAL_ERROR "QRC file is empty")
    endif()
    if (NOT ASPIA_TR_TS_FILES)
        message(FATAL_ERROR "TS_FILES files are empty")
    endif()

    if(IS_ABSOLUTE "${ASPIA_TR_QRC_FILE}")
        file(RELATIVE_PATH _qrcToTs "${CMAKE_CURRENT_SOURCE_DIR}" "${ASPIA_TR_QRC_FILE}")
    else()
        set(_qrcToTs "${ASPIA_TR_QRC_FILE}")
    endif()

    get_filename_component(_qrcToTsDir "${_qrcToTs}" DIRECTORY)

    get_filename_component(_qmFilesBinaryDir "${CMAKE_CURRENT_BINARY_DIR}/${_qrcToTsDir}" ABSOLUTE)
    # to make qt5_add_translation() work as we need
    set_source_files_properties(${ASPIA_TR_TS_FILES} PROPERTIES OUTPUT_LOCATION "${_qmFilesBinaryDir}")

    qt5_add_translation(_qmFiles ${ASPIA_TR_TS_FILES})

    set(_qrc_dest_dir "${_binaryDir}/${_qrcToTsDir}")
    set(_qrc_dest_file "${_binaryDir}/${ASPIA_TR_QRC_FILE}")

    message(STATUS "Copying ${ASPIA_TR_QRC_FILE} to ${_qrc_dest_dir}")
    file(COPY ${ASPIA_TR_QRC_FILE} DESTINATION ${_qrc_dest_dir})

    set_source_files_properties("${_qrc_dest_file}" PROPERTIES
        GENERATED True
        OBJECT_DEPENDS "${_qmFiles}")

    # With AUTORCC enabled rcc is ran by cmake before language files are generated,
    # and thus we call rcc explicitly
    qt5_add_resources(_resources "${_qrc_dest_file}")
    target_sources(${_target} PRIVATE "${_resources}")
endfunction()

function(add_qt_translations _target)
    set(oneValueArgs QRC_FILE)
    set(multiValueArgs QM_FILES)
    cmake_parse_arguments(ASPIA_TR "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_target_property(_binaryDir ${_target} BINARY_DIR)

    if (NOT ASPIA_TR_QRC_FILE)
        message(FATAL_ERROR "QRC file is empty")
    endif()
    if (NOT ASPIA_TR_QM_FILES)
        message(FATAL_ERROR "QM_FILES files are empty")
    endif()

    if(IS_ABSOLUTE "${ASPIA_TR_QRC_FILE}")
        file(RELATIVE_PATH _qrcToTs "${CMAKE_CURRENT_SOURCE_DIR}" "${ASPIA_TR_QRC_FILE}")
    else()
        set(_qrcToTs "${ASPIA_TR_QRC_FILE}")
    endif()

    get_filename_component(_qrcToTsDir "${_qrcToTs}" DIRECTORY)

    set(_qrc_dest_dir "${_binaryDir}/${_qrcToTsDir}")
    set(_qrc_dest_file "${_binaryDir}/${ASPIA_TR_QRC_FILE}")

    message(STATUS "Copying ${ASPIA_TR_QRC_FILE} to ${_qrc_dest_dir}")
    file(COPY ${ASPIA_TR_QRC_FILE} DESTINATION ${_qrc_dest_dir})

    foreach(QM_FILE ${ASPIA_TR_QM_FILES})
        file(COPY ${QM_FILE} DESTINATION ${_qrc_dest_dir})
    endforeach()

    set_source_files_properties("${_qrc_dest_file}" PROPERTIES
        GENERATED True
        OBJECT_DEPENDS "${ASPIA_TR_QM_FILES}")

    # With AUTORCC enabled rcc is ran by cmake before language files are generated,
    # and thus we call rcc explicitly
    qt5_add_resources(_resources "${_qrc_dest_file}")
    target_sources(${_target} PRIVATE "${_resources}")
endfunction()

function(CREATE_TRANSLATION _qm_files)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs OPTIONS)

    cmake_parse_arguments(_LUPDATE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(_lupdate_files ${_LUPDATE_UNPARSED_ARGUMENTS})
    set(_lupdate_options ${_LUPDATE_OPTIONS})

    set(_my_sources)
    set(_my_tsfiles)
    foreach(_file ${_lupdate_files})
        get_filename_component(_ext ${_file} EXT)
        get_filename_component(_abs_FILE ${_file} ABSOLUTE)
        if(_ext MATCHES "ts")
            list(APPEND _my_tsfiles ${_abs_FILE})
        else()
            list(APPEND _my_sources ${_abs_FILE})
        endif()
    endforeach()
    foreach(_ts_file ${_my_tsfiles})
        if(_my_sources)
            # make a list file to call lupdate on, so we don't make our commands too
            # long for some systems
            get_filename_component(_ts_name ${_ts_file} NAME_WE)
            set(_ts_lst_file "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${_ts_name}_lst_file")
            set(_lst_file_srcs)
            foreach(_lst_file_src ${_my_sources})
                set(_lst_file_srcs "${_lst_file_src}\n${_lst_file_srcs}")
            endforeach()

            set(_inc_DIRS ${PROJECT_SOURCE_DIR}/source ${PROJECT_BINARY_DIR}/source)
            foreach(_pro_include ${_inc_DIRS})
                get_filename_component(_abs_include "${_pro_include}" ABSOLUTE)
                set(_lst_file_srcs "-I${_pro_include}\n${_lst_file_srcs}")
            endforeach()

            file(WRITE ${_ts_lst_file} "${_lst_file_srcs}")
        endif()
        add_custom_command(OUTPUT ${_ts_file}
            COMMAND ${Qt5_LUPDATE_EXECUTABLE}
            ARGS ${_lupdate_options} "@${_ts_lst_file}" -ts ${_ts_file}
            DEPENDS ${_my_sources} ${_ts_lst_file} VERBATIM)
    endforeach()
    qt5_add_translation(${_qm_files} ${_my_tsfiles})
    set(${_qm_files} ${${_qm_files}} PARENT_SCOPE)
endfunction()
