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

# Appends |files| to |var|, prefixing each entry with the directory of the calling
# listfile. Lets a per-directory source list be written as a plain enumeration of
# file names instead of repeating ${CMAKE_CURRENT_LIST_DIR} on every line.
#
# Must be a macro (not a function): only then CMAKE_CURRENT_LIST_DIR resolves to the
# caller's listfile directory, and |var| is updated in the caller's scope without
# requiring PARENT_SCOPE.
macro(collect_sources var)
    foreach(_collect_sources_file ${ARGN})
        list(APPEND ${var} "${CMAKE_CURRENT_LIST_DIR}/${_collect_sources_file}")
    endforeach()
endmacro()
