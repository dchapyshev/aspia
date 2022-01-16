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

list(APPEND SOURCE_TBB_C_ALLOCATOR tbb_c_allocator.cc tbb_c_allocator.h)

source_group("" FILES ${SOURCE_TBB_C_ALLOCATOR})

add_library(tbb_c_allocator STATIC ${SOURCE_TBB_C_ALLOCATOR})
target_link_libraries(tbb_c_allocator ${TBB_LIBS})
set_target_properties(tbb_c_allocator PROPERTIES FOLDER third_party)
