#
# SmartCafe Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_PORTAUDIO pa_memorybarrier.h pa_ringbuffer.c pa_ringbuffer.h)

source_group("" FILES ${SOURCE_PORTAUDIO})

add_library(portaudio STATIC ${SOURCE_PORTAUDIO})
set_target_properties(portaudio PROPERTIES LANGUAGE C LINKER_LANGUAGE C)
set_target_properties(portaudio PROPERTIES COMPILE_DEFINITIONS "OSATOMIC_USE_INLINED")
set_target_properties(portaudio PROPERTIES FOLDER third_party)
