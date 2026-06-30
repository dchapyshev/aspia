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

collect_sources(SOURCE_BASE_WIN
    battery_enumerator.cc
    battery_enumerator.h
    desktop.cc
    desktop.h
    device.cc
    device.h
    device_enumerator.cc
    device_enumerator.h
    event_enumerator.cc
    event_enumerator.h
    file_version_info.cc
    file_version_info.h
    message_window.cc
    message_window.h
    net_share_enumerator.cc
    net_share_enumerator.h
    power_info.cc
    power_info.h
    printer_enumerator.cc
    printer_enumerator.h
    registry.cc
    registry.h
    resource_util.cc
    resource_util.h
    safe_mode_util.cc
    safe_mode_util.h
    scoped_clipboard.cc
    scoped_clipboard.h
    scoped_co_mem.h
    scoped_com_initializer.cc
    scoped_com_initializer.h
    scoped_device_info.h
    scoped_gdi_object.h
    scoped_hdc.h
    scoped_hglobal.h
    scoped_impersonator.cc
    scoped_impersonator.h
    scoped_local.h
    scoped_object.h
    scoped_select_object.h
    scoped_thread_desktop.cc
    scoped_thread_desktop.h
    scoped_user_object.h
    scoped_wts_memory.h
    security_helpers.cc
    security_helpers.h
    session_enumerator.cc
    session_enumerator.h
    session_info.cc
    session_info.h
    window_station.cc
    window_station.h
    windows_version.cc
    windows_version.h)

collect_sources(SOURCE_BASE_WIN_TESTS
    registry_unittest.cc
    scoped_object_unittest.cc)
