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

# Client-side stubs for the wlr-screencopy-unstable-v1 Wayland protocol, used by ScreenCapturerWlr.
# These were generated once from wlr-screencopy-unstable-v1.xml (kept here for reference only) with
# "wayland-scanner client-header" and "wayland-scanner private-code"; regenerate them by hand if the
# protocol XML is ever updated. wayland-util.h comes from the system libwayland-dev.

list(APPEND SOURCE_WLR_PROTOCOLS
    wlr-screencopy-unstable-v1-client-protocol.h
    wlr-screencopy-unstable-v1-protocol.c)

source_group("" FILES ${SOURCE_WLR_PROTOCOLS})

add_library(wlr_protocols STATIC ${SOURCE_WLR_PROTOCOLS})
target_include_directories(wlr_protocols PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(wlr_protocols PROPERTIES LANGUAGE C LINKER_LANGUAGE C C_STANDARD 99)
set_target_properties(wlr_protocols PROPERTIES FOLDER third_party)

# Vendored third-party code - silence its compiler warnings.
target_compile_options(wlr_protocols PRIVATE -w)
