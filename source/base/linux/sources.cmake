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

collect_sources(SOURCE_BASE_LINUX
    libdrm.cc
    libdrm.h
    libsystemd.cc
    libsystemd.h
    libxdamage.cc
    libxdamage.h
    libxfixes.cc
    libxfixes.h
    libxrandr.cc
    libxrandr.h
    libxtst.cc
    libxtst.h
    scoped_user_credentials.cc
    scoped_user_credentials.h
    session_dbus.cc
    session_dbus.h
    session_util.cc
    session_util.h
    wayland_output_layout.cc
    wayland_output_layout.h
    x11_headers.h
    x_server_clipboard.cc
    x_server_clipboard.h)

# Generate the Wayland xdg-output client stubs used by wayland_output_layout.cc to read the
# compositor's logical monitor layout. wayland-scanner and the protocol XML ship with the system
# wayland-scanner / wayland-protocols packages.
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
find_file(XDG_OUTPUT_PROTOCOL_XML
    NAMES xdg-output-unstable-v1.xml
    PATHS /usr/share/wayland-protocols /usr/local/share/wayland-protocols
    PATH_SUFFIXES unstable/xdg-output
    REQUIRED)
set(XDG_OUTPUT_CLIENT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-output-client-protocol.h")
set(XDG_OUTPUT_PROTOCOL_CODE "${CMAKE_CURRENT_BINARY_DIR}/xdg-output-protocol.c")
add_custom_command(
    OUTPUT "${XDG_OUTPUT_CLIENT_HEADER}"
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
            "${XDG_OUTPUT_PROTOCOL_XML}" "${XDG_OUTPUT_CLIENT_HEADER}"
    DEPENDS "${XDG_OUTPUT_PROTOCOL_XML}"
    VERBATIM)
add_custom_command(
    OUTPUT "${XDG_OUTPUT_PROTOCOL_CODE}"
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
            "${XDG_OUTPUT_PROTOCOL_XML}" "${XDG_OUTPUT_PROTOCOL_CODE}"
    DEPENDS "${XDG_OUTPUT_PROTOCOL_XML}"
    VERBATIM)
list(APPEND SOURCE_BASE_LINUX "${XDG_OUTPUT_CLIENT_HEADER}" "${XDG_OUTPUT_PROTOCOL_CODE}")
