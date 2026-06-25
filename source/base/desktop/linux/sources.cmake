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

collect_sources(SOURCE_BASE_DESKTOP_LINUX
    egl_dmabuf.cc
    egl_dmabuf.h
    mutter_screen_cast.cc
    mutter_screen_cast.h
    pipewire.cc
    pipewire.h
    shared_x_display.cc
    shared_x_display.h
    wayland_capture_source.h
    wayland_compositor_source.cc
    wayland_compositor_source.h
    wayland_input_target.h
    wayland_portal.cc
    wayland_portal.h
    x_atom_cache.cc
    x_atom_cache.h
    x_error_trap.cc
    x_error_trap.h
    x_server_pixel_buffer.cc
    x_server_pixel_buffer.h)

# Generate the wlr-screencopy client stubs used by ScreenCapturerWlr. The protocol XML is vendored
# (it ships with wlr-protocols, which is not packaged on the build host). CMAKE_CURRENT_* here refer to
# base/ (this file is include()'d from base/CMakeLists.txt), and base's binary dir is already on the
# include path, so the generated header is reachable by its short name.
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
set(WLR_SCREENCOPY_XML "${CMAKE_CURRENT_SOURCE_DIR}/desktop/linux/wlr-screencopy-unstable-v1.xml")
set(WLR_SCREENCOPY_HEADER "${CMAKE_CURRENT_BINARY_DIR}/wlr-screencopy-unstable-v1-client-protocol.h")
set(WLR_SCREENCOPY_CODE "${CMAKE_CURRENT_BINARY_DIR}/wlr-screencopy-unstable-v1-protocol.c")
add_custom_command(
    OUTPUT "${WLR_SCREENCOPY_HEADER}"
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
            "${WLR_SCREENCOPY_XML}" "${WLR_SCREENCOPY_HEADER}"
    DEPENDS "${WLR_SCREENCOPY_XML}"
    VERBATIM)
add_custom_command(
    OUTPUT "${WLR_SCREENCOPY_CODE}"
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
            "${WLR_SCREENCOPY_XML}" "${WLR_SCREENCOPY_CODE}"
    DEPENDS "${WLR_SCREENCOPY_XML}"
    VERBATIM)
list(APPEND SOURCE_BASE_DESKTOP_LINUX "${WLR_SCREENCOPY_HEADER}" "${WLR_SCREENCOPY_CODE}")
