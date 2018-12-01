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

                set(_inc_DIRS ${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
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
else()
    message(WARNING "Qt5 linguist tools not found. Internationalization support will be disabled.")
endif()
