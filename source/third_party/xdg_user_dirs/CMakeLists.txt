#
# SmartCafe Project
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

list(APPEND SOURCE_XDG_USER_DIRS xdg_user_dir_lookup.cc xdg_user_dir_lookup.h)

source_group("" FILES ${SOURCE_XDG_USER_DIRS})

add_library(xdg_user_dirs STATIC ${SOURCE_XDG_USER_DIRS})
set_target_properties(xdg_user_dirs PROPERTIES FOLDER third_party)