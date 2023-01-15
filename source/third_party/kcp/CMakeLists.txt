#
# Aspia Project
# Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_KCP ikcp.c ikcp.h)

source_group("" FILES ${SOURCE_KCP})

add_library(kcp STATIC ${SOURCE_KCP})
set_target_properties(kcp PROPERTIES LANGUAGE C LINKER_LANGUAGE C)
set_target_properties(kcp PROPERTIES FOLDER third_party)
