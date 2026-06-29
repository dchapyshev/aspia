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

list(APPEND SOURCE_LIBVTERM
    src/encoding.c
    src/keyboard.c
    src/mouse.c
    src/parser.c
    src/pen.c
    src/screen.c
    src/state.c
    src/unicode.c
    src/vterm.c)

source_group("" FILES ${SOURCE_LIBVTERM})

add_library(vterm STATIC ${SOURCE_LIBVTERM})
target_include_directories(vterm PUBLIC include)
set_target_properties(vterm PROPERTIES LANGUAGE C LINKER_LANGUAGE C C_STANDARD 99)
set_target_properties(vterm PROPERTIES FOLDER third_party)

# Vendored third-party code - silence its compiler warnings.
if(MSVC)
    target_compile_options(vterm PRIVATE /wd4018 /wd4244 /wd4245 /wd4267 /wd4996)
else()
    target_compile_options(vterm PRIVATE -w)
endif()
