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

function(add_tbb _target _tbb_root_dir)

    set(_tbb_bin_dir ${_tbb_root_dir}/bin)

    set(_tbb_release tbb.dll)
    set(_tbb_malloc_release tbbmalloc.dll)
    set(_tbb_malloc_proxy_release tbbmalloc_proxy.dll)

    set(_tbb_debug tbb_debug.dll)
    set(_tbb_malloc_debug tbbmalloc_debug.dll)
    set(_tbb_malloc_proxy_debug tbbmalloc_proxy_debug.dll)

    add_custom_command(TARGET ${_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_tbb_bin_dir}/$<$<CONFIG:Debug>:"${_tbb_debug}">$<$<CONFIG:Release>:"${_tbb_release}">$<$<CONFIG:RelWithDebInfo>:"${_tbb_release}">$<$<CONFIG:MinSizeRel>:"${_tbb_release}">
        "${PROJECT_BINARY_DIR}/$<CONFIGURATION>")

    add_custom_command(TARGET ${_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_tbb_bin_dir}/$<$<CONFIG:Debug>:"${_tbb_malloc_debug}">$<$<CONFIG:Release>:"${_tbb_malloc_release}">$<$<CONFIG:RelWithDebInfo>:"${_tbb_malloc_release}">$<$<CONFIG:MinSizeRel>:"${_tbb_malloc_release}">
        "${PROJECT_BINARY_DIR}/$<CONFIGURATION>")

    add_custom_command(TARGET ${_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_tbb_bin_dir}/$<$<CONFIG:Debug>:"${_tbb_malloc_proxy_debug}">$<$<CONFIG:Release>:"${_tbb_malloc_proxy_release}">$<$<CONFIG:RelWithDebInfo>:"${_tbb_malloc_proxy_release}">$<$<CONFIG:MinSizeRel>:"${_tbb_malloc_proxy_release}">
        "${PROJECT_BINARY_DIR}/$<CONFIGURATION>")

endfunction()
