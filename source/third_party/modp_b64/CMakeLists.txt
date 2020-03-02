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

list(APPEND SOURCE_MODP_B64 modp_b64.c modp_b64.h modp_b64_data.h)

source_group("" FILES ${SOURCE_MODP_B64})

add_library(modp_b64 STATIC ${SOURCE_MODP_B64})
set_target_properties(modp_b64 PROPERTIES LANGUAGE C LINKER_LANGUAGE C)
set_target_properties(modp_b64 PROPERTIES FOLDER third_party)